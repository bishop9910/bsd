#include <iostream>
#include <stduuid/uuid.h>
#include <curl/curl.h>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>

#include "thread_pool.hpp"
#include "utils.hpp"
#include "option.hpp"
#include "json.hpp"

namespace BSD_CORE {
  struct Chunk {
    std::uint64_t id    = 0;
    std::uint64_t begin = 0;   // 绝对偏移，闭区间
    std::uint64_t end   = 0;   // 绝对偏移，闭区间
    std::uint64_t done  = 0;   // 已下载字节数（从 begin 起算，不是绝对偏移）

    std::uint64_t size() const {
      return end - begin + 1;
    }

    std::uint64_t remain() const {
      return size() - done;
    }

    bool complete() const {
      return done >= size();
    }

    // 下一个要写入的绝对偏移（给 seekp 用）
    std::uint64_t next_pos() const {
      return begin + done;
    }

    // Range 请求的头 "bytes=start-end"，从当前位置开始
    std::string range_header() const {
      return "bytes=" + std::to_string(next_pos()) + "-" + std::to_string(end);
    }
  };

  class Downloader {
    public:
      Downloader(ThreadPool& tp, const Options option, std::string url):
        id(getRandomUuidGen()()),
        tp_(tp),
        thread_num(option.threads_num),
        retry(option.retry),
        timeout(option.timeout),
        file_url(url),
        headers(rawHeaderToMap(option.headers)),
        download_file_path(option.dir),
        continue_download(option.continue_download)
      {
        if(!option.output.empty()){
          file_name = option.output;
        }
      }

      ~Downloader(){
        meta_file.close();
        download_file.close();
      }

      void run();
    private:
      uuids::uuid id;
      ThreadPool& tp_; 
      size_t thread_num;
      int retry;
      int timeout;
      std::string file_url;
      std::map<std::string, std::string, CaseInsensitiveLess> headers;
      uint64_t total_bytes = 0;
      uint64_t downloaded_bytes = 0;

      std::list<Chunk> chunks;

      std::string hash;
      std::string file_name;
      std::filesystem::path download_file_path;

      bool continue_download = false;
      std::ofstream meta_file;

      std::ofstream download_file;
      
      uint64_t remaining() const { return total_bytes - downloaded_bytes; };
  };

}