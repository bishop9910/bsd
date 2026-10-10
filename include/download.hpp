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
    public:
      Chunk(size_t id, uint64_t begin, uint64_t end, std::filesystem::path temp_path, bool cache = false) :
        id(id),
        begin(begin),
        end(end),
        temp_path_(temp_path),
        cache(cache)
      {
        temp_file.open(temp_path);
      }
      Chunk(size_t id, bool is_continue){

      }
      uint64_t done  = 0;   // 该片已下多少
      std::ofstream temp_file; // 分片的temp文件，全部下载好之后拼接
      std::filesystem::path temp_path_;
      ~Chunk(){ 
        temp_file.close();
        if(cache){

        }
      }
    private:
      size_t id;
      uint64_t begin = 0;
      uint64_t end   = 0;   // 闭区间
      uint64_t need() const { return end - begin; }
      bool cache = false;
  };

  class Downloader {
    public:
      Downloader(const Options option, std::string url):
        id(getRandomUuidGen()()),
        tp(option.threads_num),
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
        download_info_file.close();
        tp.shutdown();
        download_file.close();
      }
    private:
      uuids::uuid id;
      ThreadPool tp;
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
      std::ofstream download_info_file;

      std::ofstream download_file;
      
      uint64_t remaining() const { return total_bytes - downloaded_bytes; };
  };

}