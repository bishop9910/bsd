#include <iostream>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>

#include "thread_pool.hpp"
#include "utils.hpp"
#include "option.hpp"
#include "json.hpp"
#include "log.hpp"
#include "curl.hpp"
#include "meta.hpp"
#include "hash.hpp"
#include "file_name.hpp"
#include "rate_limiter.hpp"

namespace BSD_CORE {

  class DownloadError : public std::runtime_error {
    public:
      using std::runtime_error::runtime_error;
  };
  class RetryableError : public DownloadError {
    public:
      using DownloadError::DownloadError;
  };
  class RetryableChunkError : public RetryableError {
    public:
      using RetryableError::RetryableError;
  };

  class Downloader {
    public:
      using ProgressCallback = std::function<void(std::uint64_t downloaded,
                                                  std::uint64_t total,
                                                  double bytes_per_second,
                                                  bool finished)>;

      Downloader(ThreadPool& tp, const Options& option, std::string url, std::string hash_check = "NO_HASH_CHECK", std::string output = "DEFAULT"):
        tp_(tp),
        thread_num(option.threads_num),
        file_url(url),
        download_file_path(option.dir),
        continue_download(option.continue_download)
      {
        if(output != "DEFAULT"){
          file_name = sanitize_filename(output);
        }
        if(hash_check != "NO_HASH_CHECK"){
          css = parse_checksum(hash_check);
        }
        curl_config.headers = option.headers;
        curl_config.proxy = option.proxy;
        curl_config.timeout = option.timeout;
        curl_config.user_agent = option.user_agent;

        // The rate limit is enforced globally in the sink by one shared
        // limiter; the per-connection CURLOPT limit is deliberately not used
        // any more (it would multiply the limit by the thread count).
        if(option.limit_rate_bytes > 0){
          limiter_ = std::make_shared<RateLimiter>(option.limit_rate_bytes);
        }

        file_url_md5_hex = md5_hex(file_url);
      }

      ~Downloader(){
        stop_progress();
      };

      void set_progress_callback(ProgressCallback cb) { progress_cb_ = std::move(cb); }

      // Cancellation can be requested from another thread.
      void cancel() { cancel_.store(true, std::memory_order_relaxed); }
      bool cancelled() const {
        return cancel_.load(std::memory_order_relaxed) ||
               (external_cancel_ && external_cancel_->load(std::memory_order_relaxed));
      }
      void set_external_cancel(const std::atomic<bool>* flag) { external_cancel_ = flag; }

      // Abort requested by the downloader itself (a chunk failed): the other
      // workers have to stop, but this must not be reported as a user cancel.
      void abort_workers() {
        aborted_.store(true, std::memory_order_relaxed);
        cancel_.store(true, std::memory_order_relaxed);
      }
      bool aborted() const { return aborted_.load(std::memory_order_relaxed); }

      void run();
      void prepare();
      void dispatch();
      void recover();
      void finalize();
    private:
      ThreadPool& tp_; 
      size_t thread_num;
      
      std::string file_url;
      std::string file_url_md5_hex;

      std::atomic<uint64_t> downloaded_bytes{0};
      CurlConfig curl_config;
      Meta meta_;
      Meta buildMeta(const ResponseHead& head);

      std::mutex meta_mutex_;
      std::atomic<std::uint64_t> bytes_since_save_{0};
      static constexpr size_t kMetaSaveInterval = 4 * 1024 * 1024;

      std::string file_name;
      std::filesystem::path download_file_path;

      bool continue_download = false;
      int meta_version = 2;

      std::list<std::future<void>> futures_;
      
      std::filesystem::path getMetaPath() const { return download_file_path / (file_url_md5_hex + ".meta"); }
      std::filesystem::path getPartPath() const { return download_file_path / (file_url_md5_hex + ".part"); }
      std::filesystem::path getFilePath() const { return download_file_path / PathFromUtf8(file_name); }

      std::atomic<bool> cancel_{false};
      std::atomic<bool> aborted_{false};

      void download_chunk(Chunk& c);
      void download_chunk_with_retry(Chunk& c);

      void updateMetaFile(const Meta& m);
      Meta readMetaFile();

      ResponseHead head_;

      void cleanLastFile();

      std::atomic<bool> progress_stop_{false};
      std::thread progress_thread_;
      ProgressCallback progress_cb_;
      const std::atomic<bool>* external_cancel_ = nullptr;
      std::shared_ptr<RateLimiter> limiter_;

      void start_progress();
      void stop_progress();
      void progress_loop();
      void report_progress(bool finished);

      ChecksumSpec css;
  };

}