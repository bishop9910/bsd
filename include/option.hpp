#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace BSD_CORE {
  struct Options {
    std::vector<std::string> urls;
    std::vector<std::string> outputs;
    std::string              dir = ".";
    std::size_t              threads_num = 16;
    bool                     continue_download = false;
    std::string              limit_rate;
    uint64_t                 limit_rate_bytes = 0;
    int                      retry = 3;
    int                      timeout = 90;
    std::string              proxy;
    std::vector<std::string> headers;
    std::string              user_agent;
    std::vector<std::string> checksums;
    bool                     silent   = false;
    std::string              log_file;
  };


  std::optional<Options> parse_options(int argc, char** argv);
} // namespace BSD_CORE