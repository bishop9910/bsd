#include "chunk.hpp"

namespace BSD_CORE {
  const std::uint64_t _ideal_chunk = 16 * 1024 * 1024; // 16MB
  const std::uint64_t _min_chunk   = 2 * 1024 * 1024;  // 2MB
  const std::uint64_t _max_parts   = 4096;             // 最多4096个切片

  std::vector<Range> planChunks(
      std::uint64_t total, 
      std::uint64_t ideal_chunk, 
      std::uint64_t min_chunk, 
      std::uint64_t max_parts) {
    if (total == 0) return {};
    std::uint64_t n = (total + ideal_chunk - 1) / ideal_chunk;
    n = std::clamp<std::uint64_t>(n, 1, max_parts);

    if (total >= min_chunk) {
      std::uint64_t max_by_min = total / min_chunk;
      if (n > max_by_min) n = std::max<std::uint64_t>(1, max_by_min);
    } else {
      n = 1;
    }

    std::vector<Range> out;
    out.reserve(n);
    const std::uint64_t base = total / n;
    const std::uint64_t rem  = total % n;

    std::uint64_t off = 0;

    for (std::uint64_t i = 0; i < n; ++i) {
      std::uint64_t len = base + (i < rem ? 1 : 0);
      out.push_back({off, len});
      off += len;
    }

    return out;
  }

  std::vector<Chunk> rangesToChunks(const std::vector<Range>& ranges) {
    std::vector<Chunk> chunks;
    chunks.reserve(ranges.size());

    for(const auto& range : ranges){
      chunks.push_back({range.offset, range.offset + range.length - 1, 0, 0});
    }

    return chunks;
  }
} // namespace BSD_CORE
