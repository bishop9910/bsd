#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

#ifndef BSD_CHUNK_
#define BSD_CHUNK_
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

  struct Range { std::uint64_t offset; std::uint64_t length; };

  extern const uint64_t _ideal_chunk;
  extern const uint64_t _min_chunk;
  extern const uint64_t _max_parts;

  std::vector<Range> planChunks(std::uint64_t total, std::uint64_t ideal_chunk, std::uint64_t min_chunk, std::uint64_t max_parts = 4096);
  std::vector<Chunk> rangesToChunks(const std::vector<Range>& ranges);
}
#endif