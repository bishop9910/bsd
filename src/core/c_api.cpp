#include "bsd/bsd_core.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>

#include "core.hpp"
#include "download.hpp"
#include "log.hpp"
#include "thread_pool.hpp"
#include "utils.hpp"

namespace {

  thread_local std::string tls_last_error;

  void set_error(const std::string& message) { tls_last_error = message; }

  std::mutex g_init_mutex;
  int        g_init_refs  = 0;
  bool       g_curl_ready = false;

  bsd_log_fn g_log_callback = nullptr;
  void*      g_log_user     = nullptr;

  void log_thunk(int level, const char* message, void*) {
    if (g_log_callback) g_log_callback(g_log_user, level, message);
  }

  bool valid_struct_size(std::size_t given, std::size_t expected) {
    return given == 0 || given == expected;
  }

} // namespace

struct bsd_handle {
  BSD_CORE::Options                     options;
  std::unique_ptr<BSD_CORE::ThreadPool> pool;

  std::mutex                            current_mutex;
  BSD_CORE::Downloader*                 current = nullptr;

  bool                                  init_held = false;
};

namespace {

  // Clears handle->current when bsd_download() leaves, on any path.
  struct CurrentGuard {
    bsd_handle*            handle;
    BSD_CORE::Downloader*  downloader;
    ~CurrentGuard() {
      std::lock_guard<std::mutex> lock(handle->current_mutex);
      if (handle->current == downloader) handle->current = nullptr;
    }
  };

} // namespace

extern "C" {

const char* bsd_version(void) {
  static const std::string value = BSD_CORE::versionToString(
    BSD_CORE::version, BSD_CORE::build_version, BSD_CORE::is_beta);
  return value.c_str();
}

int bsd_init(void) {
  std::lock_guard<std::mutex> lock(g_init_mutex);
  if (!g_curl_ready) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
      return -1;
    }
    g_curl_ready = true;
  }
  ++g_init_refs;
  return 0;
}

void bsd_cleanup(void) {
  bool do_cleanup = false;
  {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    if (g_init_refs > 0 && --g_init_refs == 0 && g_curl_ready) {
      g_curl_ready = false;
      do_cleanup   = true;
    }
  }
  if (do_cleanup) curl_global_cleanup();
}

bsd_handle* bsd_handle_create(const bsd_options* options) {
  if (!options) {
    set_error("bsd_handle_create: options is null");
    return nullptr;
  }
  if (!valid_struct_size(options->struct_size, sizeof(bsd_options))) {
    set_error("bsd_handle_create: unsupported bsd_options.struct_size");
    return nullptr;
  }

  if (bsd_init() != 0) {
    set_error("bsd_handle_create: curl_global_init failed");
    return nullptr;
  }

  auto handle = std::make_unique<bsd_handle>();
  handle->init_held = true;

  if (options->dir) handle->options.dir = options->dir;

  const int threads = options->threads > 0 ? options->threads : 16;
  handle->options.threads_num = static_cast<std::size_t>(
    std::clamp(threads, 1, 256));

  handle->options.retry  = options->retry > 0 ? options->retry : 3;
  handle->options.timeout =
    options->timeout_seconds > 0 ? options->timeout_seconds : 90;
  handle->options.continue_download = options->continue_download != 0;
  handle->options.limit_rate_bytes = options->limit_rate_bytes_per_second;

  if (options->proxy)      handle->options.proxy      = options->proxy;
  if (options->user_agent) handle->options.user_agent = options->user_agent;

  if (options->headers && options->header_count > 0) {
    handle->options.headers.reserve(options->header_count);
    for (std::size_t i = 0; i < options->header_count; ++i) {
      if (options->headers[i]) {
        handle->options.headers.emplace_back(options->headers[i]);
      }
    }
  }

  try {
    handle->pool = std::make_unique<BSD_CORE::ThreadPool>(
      handle->options.threads_num);
  } catch (const std::exception& e) {
    set_error(e.what());
    bsd_cleanup();
    return nullptr;
  }

  return handle.release();
}

void bsd_handle_destroy(bsd_handle* handle) {
  if (!handle) return;

  // No download may be running at this point; the pool destructor joins the
  // worker threads and drains whatever is still queued.
  handle->pool.reset();

  const bool held = handle->init_held;
  delete handle;
  if (held) bsd_cleanup();
}

void bsd_handle_cancel(bsd_handle* handle) {
  if (!handle) return;
  std::lock_guard<std::mutex> lock(handle->current_mutex);
  if (handle->current) handle->current->cancel();
}

int bsd_download(bsd_handle*        handle,
                 const bsd_request* request,
                 bsd_progress_fn    progress,
                 void*              progress_user) {
  if (!handle || !handle->pool) {
    set_error("bsd_download: invalid handle");
    return BSD_ERROR_INVALID_ARG;
  }
  if (!request || !request->url) {
    set_error("bsd_download: request.url is required");
    return BSD_ERROR_INVALID_ARG;
  }
  if (!valid_struct_size(request->struct_size, sizeof(bsd_request))) {
    set_error("bsd_download: unsupported bsd_request.struct_size");
    return BSD_ERROR_INVALID_ARG;
  }

  const std::string url(request->url);
  const std::string output =
    (request->output && *request->output) ? request->output : "DEFAULT";
  const std::string checksum =
    (request->checksum && *request->checksum) ? request->checksum : "NO_HASH_CHECK";

  bool was_cancelled = false;

  try {
    BSD_CORE::Downloader downloader(*handle->pool, handle->options, url,
                                    checksum, output);

    if (progress) {
      downloader.set_progress_callback(
        [progress, progress_user](std::uint64_t downloaded,
                                  std::uint64_t total,
                                  double bytes_per_second,
                                  bool finished) {
          try {
            progress(progress_user, downloaded, total, bytes_per_second,
                     finished ? 1 : 0);
          } catch (...) {
            // never let client code unwind through the C boundary
          }
        });
    }

    {
      std::lock_guard<std::mutex> lock(handle->current_mutex);
      handle->current = &downloader;
    }
    CurrentGuard guard{handle, &downloader};

    try {
      downloader.run();
    } catch (...) {
      was_cancelled = downloader.cancelled();
      throw;
    }

    return BSD_OK;

  } catch (const BSD_CORE::RetryableError& e) {
    if (was_cancelled) {
      set_error("cancelled");
      return BSD_ERROR_CANCELLED;
    }
    set_error(e.what());
    return BSD_ERROR_RETRYABLE;
  } catch (const BSD_CORE::DownloadError& e) {
    if (was_cancelled) {
      set_error("cancelled");
      return BSD_ERROR_CANCELLED;
    }
    set_error(e.what());
    return BSD_ERROR;
  } catch (const std::exception& e) {
    if (was_cancelled) {
      set_error("cancelled");
      return BSD_ERROR_CANCELLED;
    }
    set_error(e.what());
    return BSD_ERROR;
  } catch (...) {
    set_error("unknown error");
    return BSD_ERROR;
  }
}

const char* bsd_last_error(void) {
  return tls_last_error.c_str();
}

void bsd_set_log_silent(int silent) {
  BSD_CORE::Log::set_silent(silent != 0);
}

void bsd_set_log_console(int enabled) {
  BSD_CORE::Log::set_console(enabled != 0);
}

void bsd_set_log_level(int level) {
  const int clamped = std::clamp(level, 0, 5);
  BSD_CORE::Log::set_level(static_cast<BSD_CORE::LogLevel>(clamped));
}

void bsd_set_log_file(const char* path) {
  BSD_CORE::Log::set_file(path ? path : "");
}

void bsd_set_log_callback(bsd_log_fn callback, void* user) {
  g_log_callback = callback;
  g_log_user     = user;
  BSD_CORE::Log::set_callback(callback ? &log_thunk : nullptr, nullptr);
}

void bsd_log_message(int level, const char* message) {
  const int clamped = std::clamp(level, 0, 5);
  BSD_CORE::Log::Line line(static_cast<BSD_CORE::LogLevel>(clamped));
  line << (message ? message : "");
}

} // extern "C"
