#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace BSD_CORE {

  /*
   * Global (all worker threads share one instance) pacing limiter.
   *
   * Each acquire(n) reserves n / rate seconds of the shared timeline and
   * sleeps until its slot, so the aggregate throughput of every thread that
   * goes through the same instance is rate bytes/second. Waiting is done in
   * short slices so a cancelled download stops promptly.
   */
  class RateLimiter {
  public:
    explicit RateLimiter(std::uint64_t bytes_per_second);
    ~RateLimiter();

    RateLimiter(const RateLimiter&)            = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    std::uint64_t rate() const { return rate_; }

    /* Returns false when the download was cancelled while waiting. */
    bool acquire(std::size_t bytes, const std::atomic<bool>& cancelled);

  private:
    const std::uint64_t rate_;

    std::mutex mutex_;
    std::chrono::steady_clock::time_point next_free_{};

    bool timer_period_raised_ = false;
  };

} // namespace BSD_CORE
