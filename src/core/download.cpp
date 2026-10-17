#include "download.hpp"

namespace BSD_CORE {

  void Downloader::run() {
    Log::info() << "start to download the file${" << file_url << "}";
    if(!std::filesystem::exists(download_file_path)){
      std::filesystem::create_directories(download_file_path);
    }
    if(std::filesystem::exists(getMetaPath()) && std::filesystem::exists(getPartPath()) && continue_download) {
      try{
        Log::info() << "try to continue the download...";
        recover();
      }catch(const std::exception& e){
        Log::error() << "recover failed, exception:" << e.what();
        cleanLastFile();
        prepare();
      }
    }else{
      if(std::filesystem::exists(getMetaPath()) || std::filesystem::exists(getPartPath())){
        Log::info() << "find the last download file";
        cleanLastFile();
      }
      prepare();
    }

    if(!silent) {
      start_progress();
    }

    try{
      dispatch();
      finalize();
    }catch(...){
      stop_progress();
      throw;
    }

    stop_progress();

    if(!css.empty()){
      Log::info() << "checking file hash file${" 
        << getFilePath() << "}";
      if(verify_hash(getFilePath(), css)){
        Log::info() << "hash check passed";
      }else {
        Log::error() << "diff hash, origin: ${"
          << css.hash << "}, current: ${" 
          << compute_hash(getFilePath(), css.algo) << "}";
        Log::info() << "hash check failed";
        std::error_code ec;
        std::filesystem::remove(getFilePath(), ec);
        throw std::runtime_error("diff hash, try to redownload the file or check the hash you input");
      }
    }

    stop_progress();
  }

  void Downloader::prepare() {
    Log::info() << "preparing...";
    try{
      head_ = Curl::head(file_url, curl_config);

      if(file_name.empty()){
        file_name = resolve_filename(head_);
      }

      meta_ = buildMeta(head_);
    }catch(const std::exception& e){
      throw RetryableError(e.what());
    }
  }

  void Downloader::dispatch() {
    cancel_.store(false, std::memory_order_relaxed);

    std::uint64_t already = 0;
    for (const auto& c : meta_.chunks) already += c.done;
    downloaded_bytes.store(already, std::memory_order_relaxed);

    if (!std::filesystem::exists(getPartPath())) {
      std::ofstream create(getPartPath(), std::ios::binary | std::ios::trunc);
      if (!create)
        throw RetryableError("cannot create part file: " + getPartPath().string());
    }

    {
      std::lock_guard<std::mutex> lock(meta_mutex_);
      updateMetaFile(meta_);
    }

    for (auto& c : meta_.chunks) {
      if (c.complete()) continue;
      futures_.push_back(tp_.submit([this, &chunk = c] {
        download_chunk_with_retry(chunk);
      }));
    }

    std::exception_ptr first_error;
    for (auto& f : futures_) {
      try {
        f.get();
      } catch (...) {
        if (!first_error) {
          first_error = std::current_exception();
          cancel_.store(true, std::memory_order_relaxed);
        }
      }
    }
    futures_.clear();

    {
      std::lock_guard<std::mutex> lock(meta_mutex_);
      updateMetaFile(meta_);
    }

    if (first_error) std::rethrow_exception(first_error);

    for (auto& c : meta_.chunks) {
      if (!c.complete()) throw RetryableError("incomplete chunks after dispatch");
    }
  }

  void Downloader::recover() {
    meta_ = readMetaFile();
    head_ = Curl::head(file_url, curl_config);

    if(meta_.version != meta_version){
      throw std::runtime_error("meta file version conflict");
    }

    if (meta_.url != file_url){
      throw std::runtime_error("url mismatch, cannot resume");
    }

    if (meta_.total_bytes != head_.total_bytes()){
      throw std::runtime_error("file size changed on server");
    }

    bool etag_cmp = !meta_.etag.empty() && !head_.etag.empty();
    bool lm_cmp   = !meta_.last_modified.empty() && !head_.last_modified.empty();

    bool etag_ok = etag_cmp && meta_.etag == head_.etag;
    bool lm_ok   = lm_cmp   && meta_.last_modified == head_.last_modified;

    if (!etag_ok && !lm_ok){
      throw std::runtime_error("cannot verify server file, cannot resume");
    }

    file_name = meta_.file_name;
  }

  void Downloader::finalize() {
    std::error_code ec;
    if(std::filesystem::exists(getFilePath())) throw std::runtime_error("rename failed: file already exists");
    std::filesystem::rename(getPartPath(), getFilePath(), ec);
    if (ec) throw RetryableError("rename failed: " + ec.message());
    std::filesystem::remove(getMetaPath());
  }

  void Downloader::download_chunk(Chunk& c) {
    const std::uint64_t original_done = c.done;

    try {
      const std::uint64_t start = c.begin + c.done;
      const std::uint64_t end = c.end;
      if (start > end) return;

      const auto part_path = getPartPath();

      std::fstream fs(part_path, std::ios::binary | std::ios::in | std::ios::out);
      if (!fs) throw RetryableChunkError("cannot open part file");
      fs.seekp(static_cast<std::streamoff>(start));
      if (!fs.good()) throw RetryableChunkError("seekp failed");

      std::string if_range;
      if (!meta_.etag.empty() && meta_.etag.rfind("W/", 0) != 0)
        if_range = meta_.etag;
      else if (!meta_.last_modified.empty())
        if_range = meta_.last_modified;

      std::string sink_error;
      SinkFn sink = [&](const char* data, std::size_t len) -> bool {
        if (cancel_.load(std::memory_order_relaxed)) {
          sink_error = "cancelled";
          return false;
        }
        try {
          if (len > c.remain()) {
            sink_error = "server returned more bytes than requested";
            return false;
          }
          fs.write(data, static_cast<std::streamsize>(len));
          if (!fs.good()) {
            sink_error = "write failed";
            return false;
          }
          c.done += len;
          downloaded_bytes.fetch_add(len, std::memory_order_relaxed);
          return true;
        } catch (const std::exception& e) {
          sink_error = e.what();
          return false;
        } catch (...) {
          sink_error = "unknown";
          return false;
        }
      };

      RangeResult r = Curl::get_range(file_url, curl_config, start, end, if_range, sink);

      fs.flush();
      if (!fs.good()) throw RetryableChunkError("flush failed");

      if (!sink_error.empty()) throw RetryableChunkError(sink_error);

      if (r.status == 200)
        throw RetryableError("server returned 200 (file changed or Range unsupported)");
      if (r.status == 416)
        throw RetryableError("range not satisfiable, meta stale");
      if (r.status != 206)
        throw RetryableChunkError("HTTP " + std::to_string(r.status));

    } catch (...) {
      const std::uint64_t written = c.done - original_done;
      c.done = original_done;
      if (written > 0)
        downloaded_bytes.fetch_sub(written, std::memory_order_relaxed);
      throw;
    }
  }

  void Downloader::download_chunk_with_retry(Chunk& c) {
    constexpr int MaxAttempts = 3;

    for (int attempt = 1; ; ++attempt) {
      if (cancel_.load(std::memory_order_relaxed))
        throw RetryableChunkError("cancelled");

      try {
        download_chunk(c);
        return;
      } catch (const RetryableChunkError& e) {
        if (attempt >= MaxAttempts) {
          Log::error() << "chunk [" << c.begin << "," << c.end
            << "] failed after " << MaxAttempts
            << " attempts: " << e.what();
          throw;
        }
          Log::warn() << "chunk [" << c.begin << "," << c.end
            << "] attempt " << attempt << "/" << MaxAttempts
            << " failed: " << e.what() << ", retrying...";
          std::this_thread::sleep_for(std::chrono::milliseconds(500 * attempt));
      }
    }
  }

  Meta Downloader::readMetaFile() {
    std::ifstream ifs(getMetaPath(), std::ios::binary);
    if (!ifs) { 
      throw std::runtime_error("cannot find meta file");
    }
    std::string json_text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    Meta meta = from_json(BS_JSON::Json::parse(json_text));
    return meta;
  }

  void Downloader::cleanLastFile() {
    Log::info() << "removing the last meta file and part file";
    std::filesystem::remove(getMetaPath());
    std::filesystem::remove(getPartPath());
  }

  void Downloader::updateMetaFile(const Meta& m) {

    auto tmp = getMetaPath();
    tmp += ".tmp";
    {
      std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
      ofs << to_json(m).dump();
    }
    std::filesystem::rename(tmp, getMetaPath());
  }

  Meta Downloader::buildMeta(const ResponseHead& head) {
    Meta m;
    m.version = meta_version;
    m.url     = file_url;

    m.total_bytes   = head.total_bytes();
    m.etag          = head.etag;
    m.last_modified = head.last_modified;
    m.accept_ranges = head.accept_ranges;
    m.created_at    = now_string();

    if (m.total_bytes == 0)
      throw std::runtime_error("cannot determine file size: " + file_url);

    if (!file_name.empty()) {
      m.file_name = sanitize_filename(file_name);
    } else {
      m.file_name = resolve_filename(head);
    }
    if (m.file_name.empty())
      m.file_name = "index.html";
    

    m.chunks = rangesToChunks(planChunks(m.total_bytes, _ideal_chunk, _min_chunk, _max_parts));

    return m;
  }

  void Downloader::start_progress() {
    progress_stop_.store(false, std::memory_order_relaxed);
    progress_thread_ = std::thread([this] { progress_loop(); });
  }

  void Downloader::stop_progress() {
    progress_stop_.store(true, std::memory_order_relaxed);
    if (progress_thread_.joinable()) {
      progress_thread_.join();
    }
  }

  void Downloader::progress_loop() {
    using namespace std::chrono;

    auto last_t = steady_clock::now();
    std::uint64_t last = downloaded_bytes.load(std::memory_order_relaxed);

    while (!progress_stop_.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(milliseconds(500));

      std::uint64_t now = downloaded_bytes.load(std::memory_order_relaxed);
      auto t = steady_clock::now();

      double dt    = duration<double>(t - last_t).count();
      double speed = dt > 0 ? (now - last) / dt : 0.0;

      double pct = meta_.total_bytes > 0
                ? 100.0 * static_cast<double>(now) / static_cast<double>(meta_.total_bytes)
                : 0.0;

      double eta = (speed > 0 && meta_.total_bytes > now)
                ? (meta_.total_bytes - now) / speed
                : 0.0;

      std::fprintf(stderr, "\r%5.1f%%  %s / %s  %s/s  ETA %s   ",
                  pct,
                  human_size(static_cast<double>(now)).c_str(),
                  human_size(static_cast<double>(meta_.total_bytes)).c_str(),
                  human_size(speed).c_str(),
                  human_time(eta).c_str());
      std::fflush(stderr);

      last   = now;
      last_t = t;
    }

    std::fprintf(stderr, "\n");
  }



} // namespace BSD_CORE
