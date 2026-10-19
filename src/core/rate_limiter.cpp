#include "rate_limiter.hpp"

#include <algorithm>
#include <thread>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <timeapi.h>
#endif

namespace BSD_CORE {

  namespace {
    using clock = std::chrono::steady_clock;
    using duration = clock::duration;

    // Sleep granularity on Windows is ~15.6 ms unless the timer period is
    // raised. 1 ms is enough for rate limiting and avoids spinning.
    constexpr auto kMinSleep = std::chrono::milliseconds(1);
    constexpr auto kMaxSleep = std::chrono::milliseconds(50);
  }

  RateLimiter::RateLimiter(std::uint64_t bytes_per_second)
    : rate_(bytes_per_second) {
#if defined(_WIN32)
    if (rate_ > 0) {
      timer_period_raised_ = (timeBeginPeriod(1) == TIMERR_NOERROR);
    }
#endif
  }

  RateLimiter::~RateLimiter() {
#if defined(_WIN32)
    if (timer_period_raised_) timeEndPeriod(1);
#endif
  }

  bool RateLimiter::acquire(std::size_t bytes, const std::atomic<bool>& cancelled) {
    if (cancelled.load(std::memory_order_relaxed)) return false;
    if (rate_ == 0 || bytes == 0) return true;

    clock::time_point deadline;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto now = clock::now();
      // Do not build up a backlog while nothing is downloading.
      const auto start = std::max(next_free_, now);
      const auto cost = std::chrono::duration_cast<duration>(
        std::chrono::duration<double>(static_cast<double>(bytes) /
                                      static_cast<double>(rate_)));
      deadline = start + cost;
      next_free_ = deadline;
    }

    for (;;) {
      if (cancelled.load(std::memory_order_relaxed)) return false;

      const auto now = clock::now();
      if (now >= deadline) break;

      auto remaining = std::chrono::duration_cast<duration>(deadline - now);
      // Below one sleep granule: take the tiny burst instead of oversleeping
      // which would throttle the connection much harder than requested.
      if (remaining <= kMinSleep) break;

      const auto slice = std::min(
        remaining, std::chrono::duration_cast<duration>(kMaxSleep));
      std::this_thread::sleep_for(slice);
    }

    return !cancelled.load(std::memory_order_relaxed);
  }

} // namespace BSD_CORE
