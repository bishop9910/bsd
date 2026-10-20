#include "download.hpp"

namespace BSD_CORE {

  void Downloader::run() {
    Log::info() << "start to download the file${" << file_url << "}";
    if(file_url.empty()){
      throw DownloadError("Empty download url");
    }
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

    if(progress_cb_) {
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
    report_progress(true);
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
    bytes_since_save_.store(0, std::memory_order_relaxed);

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
      try{
        futures_.push_back(tp_.submit([this, &chunk = c] {
          download_chunk_with_retry(chunk);
        }));
      }catch (const std::exception&) {
        abort_workers();
        for (auto& f : futures_) { try { f.get(); } catch (...) {} }
        futures_.clear();
        throw;
      }
    }

    std::exception_ptr first_error;
    for (auto& f : futures_) {
      try {
        f.get();
      } catch (...) {
        if (!first_error) {
          first_error = std::current_exception();
          abort_workers();
        }
      }
    }
    futures_.clear();

    {
      std::lock_guard<std::mutex> lock(meta_mutex_);
      for (auto& c : meta_.chunks) {
        if (c.complete()) c.done_saved = c.size();
      }
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

    auto new_size = head_.total_bytes();
    if (new_size != 0 && new_size != meta_.total_bytes)
      throw std::runtime_error("file size changed on server");
    if (new_size == 0){
      Log::warn() << "server did not return Content-Length, skipping size check";
    }

    bool etag_cmp = !meta_.etag.empty() && !head_.etag.empty();
    bool lm_cmp   = !meta_.last_modified.empty() && !head_.last_modified.empty();

    if (etag_cmp && meta_.etag != head_.etag)
      throw std::runtime_error("ETag changed on server, cannot resume");
    if (!etag_cmp && lm_cmp && meta_.last_modified != head_.last_modified)
      throw std::runtime_error("Last-Modified changed on server, cannot resume");

    if (!etag_cmp && !lm_cmp) {
      Log::warn() << "server returned neither ETag nor Last-Modified; " << "resuming with only size+url check, may produce a corrupted file";
    }

    Log::info() << "the download start to continue";
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
        if (cancelled()) {
          sink_error = "cancelled";
          return false;
        }
        try {
          if (len > c.remain()) {
            sink_error = "server returned more bytes than requested";
            return false;
          }
          if (limiter_ && !limiter_->acquire(len, cancel_)) {
            sink_error = "cancelled";
            return false;
          }
          if (cancelled()) {
            sink_error = "cancelled";
            return false;
          }
          fs.write(data, static_cast<std::streamsize>(len));
          if (!fs.good()) {
            sink_error = "write failed";
            return false;
          }
          c.done += len;
          downloaded_bytes.fetch_add(len, std::memory_order_relaxed);

          const auto prev = bytes_since_save_.fetch_add(len, std::memory_order_relaxed);
          if (prev + len >= kMetaSaveInterval) {
            std::uint64_t expected = prev + len;
            if (bytes_since_save_.compare_exchange_strong(
                  expected, 0, std::memory_order_relaxed)) {
              fs.flush();
              if (!fs.good()) {
                sink_error = "flush failed before meta save";
                return false;
              }
              std::lock_guard<std::mutex> lock(meta_mutex_);
              c.done_saved = c.done;
              updateMetaFile(meta_);
            }
          }
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

      if (r.status == 200)
        throw RetryableError("server returned 200 (file changed or Range unsupported)");
      if (r.status == 416)
        throw RetryableError("range not satisfiable, meta stale");
      if (r.status != 206)
        throw RetryableChunkError("HTTP " + std::to_string(r.status));

      auto expect_begin = "bytes " + std::to_string(start) + "-";
      if (r.content_range.rfind(expect_begin, 0) != 0)
        throw RetryableChunkError("unexpected Content-Range: " + r.content_range);

      const std::uint64_t expect_size = end - start + 1;
      if (r.received != expect_size)
        throw RetryableChunkError("received " + std::to_string(r.received) + " bytes, expected " + std::to_string(expect_size));
      
      fs.flush();
      if (!fs.good()) throw RetryableChunkError("flush failed");

      if (!sink_error.empty()) throw RetryableChunkError(sink_error);

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
      if (cancelled())
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
    

    m.chunks = rangesToChunks(planChunksAdaptive(m.total_bytes, thread_num));

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

      if (progress_cb_) {
        try {
          progress_cb_(now, meta_.total_bytes, speed, false);
        } catch (...) {
          // A misbehaving client callback must not kill the progress thread.
        }
      }

      last   = now;
      last_t = t;
    }
  }

  void Downloader::report_progress(bool finished) {
    if (!progress_cb_) return;
    try {
      progress_cb_(downloaded_bytes.load(std::memory_order_relaxed),
                   meta_.total_bytes, 0.0, finished);
    } catch (...) {}
  }



} // namespace BSD_CORE
