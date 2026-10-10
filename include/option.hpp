#include <string>
#include <vector>

struct Options {
  std::vector<std::string> urls;
  std::string output;
  std::string dir = ".";
  size_t threads_num = 16;
  bool continue_download = false;
  std::string limit_rate;
  int retry = 3;
  int timeout = 30;
  std::string proxy;
  std::vector<std::string> headers;
  std::string user_agent;
  std::string checksum;
};