#include "chunk.hpp"

namespace BSD_CORE {
  const std::uint64_t _ideal_chunk = 16 * 1024 * 1024; // 16MB
  const std::uint64_t _min_chunk   = 2 * 1024 * 1024;  // 2MB
  const std::uint64_t _max_parts   = 4096;             // 最多4096个切片

  namespace {
    // Slices per worker for the adaptive plan: enough tasks to keep every
    // thread busy and to make the tail (last slice per thread) short.
    constexpr std::uint64_t kSlicesPerThread = 8;
    // Do not create silly tiny slices for small files.
    constexpr std::uint64_t kMinAdaptiveChunk = 256 * 1024; // 256KB

    std::vector<Range> splitEvenly(std::uint64_t total, std::uint64_t n) {
      std::vector<Range> out;
      out.reserve(static_cast<std::size_t>(n));

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
  }

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

    return splitEvenly(total, n);
  }

  std::vector<Range> planChunksAdaptive(std::uint64_t total, std::size_t threads) {
    if (total == 0) return {};

    const std::uint64_t workers = std::max<std::size_t>(threads, 1);
    std::uint64_t target = workers * kSlicesPerThread;
    target = std::clamp<std::uint64_t>(target, 1, _max_parts);

    std::uint64_t chunk_size = (total + target - 1) / target;
    chunk_size = std::clamp<std::uint64_t>(chunk_size, kMinAdaptiveChunk, _ideal_chunk);

    std::uint64_t n = (total + chunk_size - 1) / chunk_size;
    n = std::clamp<std::uint64_t>(n, 1, _max_parts);

    return splitEvenly(total, n);
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
