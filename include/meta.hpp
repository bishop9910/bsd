#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "json.hpp"

namespace BSD_CORE {

  struct Chunk {
    std::uint64_t begin = 0;
    std::uint64_t end   = 0;
    std::uint64_t done  = 0;

    std::uint64_t size() const { return end - begin + 1; }
    std::uint64_t remain() const { return size() - done; }
    bool complete() const { return done >= size(); }
    std::uint64_t next_pos() const { return begin + done; }
  };

  struct Meta {
    int                       version = 1;
    std::string               url;
    std::string               file_name;
    std::uint64_t             total_bytes = 0;
    std::uint64_t             chunk_size  = 0;
    std::string               etag;
    std::string               last_modified;
    std::string               accept_ranges;
    std::string               created_at;
    std::vector<Chunk>        chunks;
  };

  BS_JSON::Json to_json(const Meta& m);
  Meta from_json(const BS_JSON::Json& j);

} // namespace BSD_CORE