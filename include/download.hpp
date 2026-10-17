#include <iostream>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>

#include "thread_pool.hpp"
#include "utils.hpp"
#include "option.hpp"
#include "json.hpp"
#include "log.hpp"
#include "curl.hpp"
#include "meta.hpp"
#include "hash.hpp"
#include "file_name.hpp"

namespace BSD_CORE {

  class DownloadError : public std::runtime_error {
    public:
      using std::runtime_error::runtime_error;
  };
  class RetryableError : public DownloadError {
    public:
      using DownloadError::DownloadError;
  };
  class RetryableChunkError : public DownloadError {
    public:
      using DownloadError::DownloadError;
  };

  class Downloader {
    public:
      Downloader(ThreadPool& tp, const Options& option, std::string url, std::string hash_check = "NO_HASH_CHECK", std::string output = "DEFAULT"):
        tp_(tp),
        thread_num(option.threads_num),
        file_url(url),
        download_file_path(option.dir),
        continue_download(option.continue_download),
        silent(option.silent)
      {
        if(output != "DEFAULT"){
          file_name = sanitize_filename(output);
        }
        if(hash_check != "NO_HASH_CHECK"){
          css = parse_checksum(hash_check);
        }
        curl_config.headers = option.headers;
        curl_config.limit_rate = option.limit_rate_bytes;
        curl_config.proxy = option.proxy;
        curl_config.timeout = option.timeout;
        curl_config.user_agent = option.user_agent;
        // curl_config.verify_ssl = true;
      }

      ~Downloader(){
        stop_progress();
      };

      void run();
      void prepare();
      void dispatch();
      void recover();
      void finalize();
    private:
      ThreadPool& tp_; 
      size_t thread_num;
      std::string file_url;
      std::atomic<uint64_t> downloaded_bytes{0};
      CurlConfig curl_config;
      Meta meta_;
      Meta buildMeta(const ResponseHead& head);

      std::mutex meta_mutex_;
      std::atomic<std::uint64_t> bytes_since_save_{0};

      static constexpr std::uint64_t kMetaSaveInterval = 4 * 1024 * 1024;  // 4MB

      std::string file_name;
      std::filesystem::path download_file_path;

      bool continue_download = false;
      int meta_version = 2;

      std::list<std::future<void>> futures_;
      
      std::filesystem::path getMetaPath() const { return download_file_path / (md5_hex(file_url) + ".meta"); }
      std::filesystem::path getPartPath() const { return download_file_path / (md5_hex(file_url) + ".part"); }
      std::filesystem::path getFilePath() const { return download_file_path / PathFromUtf8(file_name); }
      uint64_t remaining() const {
        auto d = downloaded_bytes.load(std::memory_order_relaxed);
        return meta_.total_bytes > d ? meta_.total_bytes - d : 0;
      }

      std::atomic<bool> cancel_{false};
      
      void download_chunk(Chunk& c);
      void download_chunk_with_retry(Chunk& c);

      void updateMetaFile(const Meta& m);
      Meta readMetaFile();

      ResponseHead head_;

      void cleanLastFile();

      bool silent = false;
      std::atomic<bool> progress_stop_{false};
      std::thread progress_thread_;

      void start_progress();
      void stop_progress();
      void progress_loop();

      ChecksumSpec css;
  };

}