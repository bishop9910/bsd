#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "json.hpp"
#include "chunk.hpp"

namespace BSD_CORE {

  struct Meta {
    int                       version = 0;
    std::string               url;
    std::string               file_name;
    std::uint64_t             total_bytes = 0;
    std::string               etag;
    std::string               last_modified;
    std::string               accept_ranges;
    std::string               created_at;
    std::vector<Chunk>        chunks;
  };

  BS_JSON::Json to_json(const Meta& m);
  Meta from_json(const BS_JSON::Json& j);

} // namespace BSD_CORE