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

namespace BSD_CORE {

  class Downloader {
    public:
      Downloader(ThreadPool& tp, const Options option, std::string url):
        tp_(tp),
        thread_num(option.threads_num),
        retry(option.retry),
        file_url(url),
        download_file_path(option.dir),
        continue_download(option.continue_download),
        silent(option.silent)
      {
        if(!option.output.empty()){
          file_name = option.output;
        }
        curl_config.headers = option.headers;
        curl_config.limit_rate = option.limit_rate_bytes;
        curl_config.proxy = option.proxy;
        curl_config.timeout = option.timeout;
        curl_config.user_agent = option.user_agent;
        // curl_config.verify_ssl = true;
      }

      ~Downloader(){
        if(meta_.version == 1){
          updateMetaFile(meta_);
        }

        if(!progress_stop_){
          stop_progress();
        }
      };

      void run();
      void prepare();
      void dispatch();
      void recover();
      void finalize();
    private:
      ThreadPool& tp_; 
      size_t thread_num;
      int retry;
      int timeout;
      std::string file_url;
      uint64_t total_bytes = 0;
      std::atomic<uint64_t> downloaded_bytes{0};
      CurlConfig curl_config;
      Meta meta_;
      Meta buildMeta(const ResponseHead& head);

      void createChunks(Meta& m, std::uint64_t total, std::uint64_t chunk_size) const;

      std::string file_name;
      std::filesystem::path download_file_path;

      bool continue_download = false;
      double meta_verison = 1.0;

      std::ofstream download_file;

      std::list<std::future<void>> futures_;
      
      std::filesystem::path getMetaPath() const { return download_file_path / (file_name + ".meta"); }
      std::filesystem::path getPartPath() const { return download_file_path / (file_name + ".part"); }
      std::filesystem::path getFilePath() const { return download_file_path / file_name; }
      uint64_t remaining() const { return total_bytes - downloaded_bytes; }

      void updateMetaFile(Meta& m);
      Meta readMetaFile();
      Meta buildMeta(const ResponseHead& head) const;

      ResponseHead head_;

      void cleanLastFile();

      bool silent = false;
      std::atomic<bool> progress_stop_{false};
      std::thread progress_thread_;

      void start_progress();
      void stop_progress();
      void progress_loop();
  };

}