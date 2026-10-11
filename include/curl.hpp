#pragma once

#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "utils.hpp"

namespace BSD_CORE {

struct CurlConfig {
  int                      timeout    = 30;
  std::string              proxy;
  std::string              user_agent;
  std::vector<std::string> headers;
  std::uint64_t            limit_rate = 0;
  bool                     verify_ssl = true;
};

struct ResponseHead {
  long        status = 0;
  std::string url;
  std::string content_type;
  std::string content_length;
  std::string content_disposition;
  std::string content_range;
  std::string etag;
  std::string last_modified;
  std::string accept_ranges;
  std::string location;
  std::map<std::string, std::string, CaseInsensitiveLess> extra;

  std::uint64_t total_bytes() const;
  bool          supports_range() const;
};

struct RangeResult {
  long          status   = 0;
  std::uint64_t received = 0;
  std::string   content_range;
};

using SinkFn = std::function<bool(const char* data, std::size_t len)>;

class Curl {
public:
  Curl() = delete;

  static void global_init();
  static void global_cleanup();

  static ResponseHead head(const std::string& url, const CurlConfig& cfg);

  static RangeResult get_range(const std::string& url,
                               const CurlConfig& cfg,
                               std::uint64_t start,
                               std::uint64_t end,
                               const std::string& if_range,
                               const SinkFn& sink);

  static RangeResult get_range_to_file(const std::string& url,
                                       const CurlConfig& cfg,
                                       std::uint64_t start,
                                       std::uint64_t end,
                                       const std::string& if_range,
                                       std::ofstream& ofs);
};

} // namespace BSD_CORE