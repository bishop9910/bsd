#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "bsd/bsd_core.h"
#include "option.hpp"

namespace {

  std::string join(const std::vector<std::string>& values, const char* sep = ", ") {
    std::ostringstream os;
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (i) os << sep;
      os << values[i];
    }
    return os.str();
  }

  std::string human_size(double bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    while (bytes >= 1024.0 && i < 4) { bytes /= 1024.0; ++i; }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f%s", bytes, units[i]);
    return buf;
  }

  std::string human_time(double sec) {
    if (sec <= 0 || sec > 99 * 3600) return "--:--";
    int s = static_cast<int>(sec);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
    return buf;
  }

  void log_line(int level, const std::string& message) {
    bsd_log_message(level, message.c_str());
  }

  void dump_options(const BSD_CORE::Options& o) {
    log_line(BSD_LOG_DEBUG, "===== Options =====");
    log_line(BSD_LOG_DEBUG, "urls            = [" + join(o.urls) + "]");
    log_line(BSD_LOG_DEBUG, "output          = " + (o.outputs.empty() ? std::string("(auto)") : "[" + join(o.outputs) + "]"));
    log_line(BSD_LOG_DEBUG, "dir             = " + o.dir);
    log_line(BSD_LOG_DEBUG, "threads_num     = " + std::to_string(o.threads_num));
    log_line(BSD_LOG_DEBUG, "continue        = " + std::string(o.continue_download ? "true" : "false"));
    log_line(BSD_LOG_DEBUG, "limit_rate      = " + (o.limit_rate.empty() ? std::string("(none)") : o.limit_rate));
    log_line(BSD_LOG_DEBUG, "retry           = " + std::to_string(o.retry));
    log_line(BSD_LOG_DEBUG, "timeout         = " + std::to_string(o.timeout) + "s");
    log_line(BSD_LOG_DEBUG, "proxy           = " + (o.proxy.empty() ? std::string("(none)") : o.proxy));
    log_line(BSD_LOG_DEBUG, "headers         = [" + join(o.headers) + "]");
    log_line(BSD_LOG_DEBUG, "user_agent      = " + (o.user_agent.empty() ? std::string("(default)") : o.user_agent));
    log_line(BSD_LOG_DEBUG, "checksums       = " + (o.checksums.empty() ? std::string("(none)") : "[" + join(o.checksums) + "]"));
    log_line(BSD_LOG_DEBUG, "silent          = " + std::string(o.silent ? "true" : "false"));
    log_line(BSD_LOG_DEBUG, "log_file        = " + (o.log_file.empty() ? std::string("(console only)") : o.log_file));
    log_line(BSD_LOG_DEBUG, "===================");
  }

  struct ProgressState {
    bool started  = false;
    bool finished = false;
  };

} // namespace

// extern "C" so the pointer type matches bsd_progress_fn exactly.
extern "C" void cli_progress_cb(void* user, uint64_t downloaded, uint64_t total,
                                double bytes_per_second, int finished) {
  auto* state = static_cast<ProgressState*>(user);
  state->started = true;

  const double pct = total > 0
                   ? 100.0 * static_cast<double>(downloaded) / static_cast<double>(total)
                   : 0.0;
  const double eta = (bytes_per_second > 0 && total > downloaded)
                   ? static_cast<double>(total - downloaded) / bytes_per_second
                   : 0.0;

  std::fprintf(stderr, "\r%5.1f%%  %s / %s  %s/s  ETA %s   ",
               pct,
               human_size(static_cast<double>(downloaded)).c_str(),
               human_size(static_cast<double>(total)).c_str(),
               human_size(bytes_per_second).c_str(),
               human_time(eta).c_str());
  std::fflush(stderr);

  if (finished) {
    std::fprintf(stderr, "\n");
    state->finished = true;
  }
}

int main(int argc, char** argv) {
  std::srand(static_cast<unsigned>(std::time(nullptr)));

  std::optional<BSD_CORE::Options> parsed;

  try {
    parsed = BSD_CORE::parse_options(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "exception: %s\n", e.what());
    return 1;
  }

  if (!parsed) return 0;

  BSD_CORE::Options opts = std::move(*parsed);

  if (bsd_init() != 0) {
    std::fprintf(stderr, "failed to initialize bsd_core\n");
    return 1;
  }

  struct CoreCleanup {
    ~CoreCleanup() { bsd_cleanup(); }
  } core_cleanup;

  bsd_set_log_silent(opts.silent ? 1 : 0);
  if (!opts.log_file.empty()) {
    bsd_set_log_file(opts.log_file.c_str());
  }
  bsd_set_log_level(BSD_LOG_INFO);

  dump_options(opts);

  if (!opts.checksums.empty() && opts.checksums.size() != opts.urls.size()) {
    log_line(BSD_LOG_ERROR, "num of hash check list is not as same as url list, invaild hash check request");
    log_line(BSD_LOG_WARN, "clear the invaild check list");
    opts.checksums.clear();
  }

  if (!opts.outputs.empty() && opts.outputs.size() != opts.urls.size()) {
    log_line(BSD_LOG_ERROR, "num of hash output list is not as same as url list, invaild output request");
    log_line(BSD_LOG_WARN, "clear the invaild output list");
    opts.outputs.clear();
  }

  std::vector<const char*> header_ptrs;
  header_ptrs.reserve(opts.headers.size());
  for (const auto& header : opts.headers) header_ptrs.push_back(header.c_str());

  bsd_options core_options{};
  core_options.struct_size = sizeof(core_options);
  core_options.dir = opts.dir.c_str();
  core_options.threads = static_cast<int>(opts.threads_num);
  core_options.retry = opts.retry;
  core_options.timeout_seconds = opts.timeout;
  core_options.continue_download = opts.continue_download ? 1 : 0;
  core_options.limit_rate_bytes_per_second = opts.limit_rate_bytes;
  core_options.proxy = opts.proxy.empty() ? nullptr : opts.proxy.c_str();
  core_options.user_agent = opts.user_agent.empty() ? nullptr : opts.user_agent.c_str();
  core_options.headers = header_ptrs.empty() ? nullptr : header_ptrs.data();
  core_options.header_count = header_ptrs.size();

  bsd_handle* handle = bsd_handle_create(&core_options);
  if (!handle) {
    log_line(BSD_LOG_ERROR, std::string("failed to create download handle: ") + bsd_last_error());
    return 1;
  }

  std::size_t failed = 0;

  for (std::size_t i = 0; i < opts.urls.size(); ++i) {
    const std::string& url = opts.urls[i];

    const std::string checksum = opts.checksums.empty() ? std::string() : opts.checksums[i];
    const std::string output = opts.outputs.empty() ? std::string() : opts.outputs[i];

    bsd_request request{};
    request.struct_size = sizeof(request);
    request.url = url.c_str();
    request.output = output.empty() ? nullptr : output.c_str();
    request.checksum = checksum.empty() ? nullptr : checksum.c_str();

    const int max_attempts = opts.retry;
    bool ok = false;

    for (int attempt = 0; attempt < max_attempts + 1; ++attempt) {
      ProgressState progress_state;
      const int rc = bsd_download(handle, &request,
                                  opts.silent ? nullptr : &cli_progress_cb,
                                  &progress_state);

      if (progress_state.started && !progress_state.finished) {
        std::fputc('\n', stderr);
      }

      if (rc == BSD_OK) {
        ok = true;
        log_line(BSD_LOG_INFO, "download finished: " + url);
        break;
      }

      const std::string error = bsd_last_error();

      if (rc == BSD_ERROR_CANCELLED) {
        log_line(BSD_LOG_WARN, "download cancelled: " + url);
        break;
      }

      if (rc == BSD_ERROR_RETRYABLE) {
        log_line(BSD_LOG_WARN,
                 "attempt " + std::to_string(attempt + 1) + "/" +
                 std::to_string(max_attempts + 1) + " failed: " + error);

        if (attempt + 1 < max_attempts + 1) {
          auto backoff_ms = (1 << attempt) * 1000 + (std::rand() % 500);
          std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
        }
        continue;
      }

      log_line(BSD_LOG_ERROR,
               "download failed (not retryable): " + url + " : " + error);
      break;
    }

    if (!ok) {
      ++failed;
      log_line(BSD_LOG_ERROR, "download gave up: " + url);
    }
  }

  bsd_handle_destroy(handle);

  log_line(BSD_LOG_INFO, "all done");
  return failed == 0 ? 0 : 1;
}
