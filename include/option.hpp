#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace BSD_CORE {
  struct Options {
    std::vector<std::string> urls;
    std::string              output;
    std::string              dir = ".";
    std::size_t              threads_num = 16;
    bool                     continue_download = false;
    std::string              limit_rate;
    uint64_t                 limit_rate_bytes = 0;
    int                      retry = 3;
    int                      timeout = 30;
    std::string              proxy;
    std::vector<std::string> headers;
    std::string              user_agent;
    std::string              checksum;
    bool                     silent   = false;
    std::string              log_file;
  };


  // 返回值：
  //   解析成功 → std::optional<Options> 有值
  //   --help / --version 触发退出 → 返回 nullopt，调用方直接 return 0
  //   解析失败 → 抛 std::runtime_error
  std::optional<Options> parse_options(int argc, char** argv);
} // namespace BSD_CORE