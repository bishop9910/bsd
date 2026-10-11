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

  class RetryableError : public std::runtime_error { 
    public:
      RetryableError(const std::string& str):runtime_error(str){};
      RetryableError(const char* str):runtime_error(str){};
      bool retryable = true;
  };

  class Downloader {
    public:
      Downloader(ThreadPool& tp, const Options& option, std::string url, std::string hash_check = "NO_HASH_CHECK"):
        tp_(tp),
        thread_num(option.threads_num),
        file_url(url),
        download_file_path(option.dir),
        continue_download(option.continue_download),
        silent(option.silent)
      {
        if(!option.output.empty()){
          file_name = sanitize_filename(option.output);
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

      void createChunks(Meta& m, std::uint64_t total, std::uint64_t chunk_size) const;

      std::string file_name;
      std::filesystem::path download_file_path;

      bool continue_download = false;
      int meta_version = 1;

      std::list<std::future<void>> futures_;
      
      std::filesystem::path getMetaPath() const { return download_file_path / (md5_hex(file_url) + ".meta"); }
      std::filesystem::path getPartPath() const { return download_file_path / (md5_hex(file_url) + ".part"); }
      std::filesystem::path getFilePath() const { return download_file_path / file_name; }
      uint64_t remaining() const {
        auto d = downloaded_bytes.load(std::memory_order_relaxed);
        return meta_.total_bytes > d ? meta_.total_bytes - d : 0;
      }

      void updateMetaFile(const Meta& m);
      Meta readMetaFile();

      void download_chunk(Chunk& c);

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