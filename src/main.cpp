#include <iostream>
#include "thread_pool.hpp"
#include "core.hpp"
#include "utils.hpp"
#include "option.hpp"
#include "log.hpp"
#include "download.hpp"
#include <curl/curl.h>

void dump_options(const BSD_CORE::Options& o) {
  using BSD_CORE::Log;
  using BSD_CORE::vector_join;

  Log::debug() << "===== Options =====";
  Log::debug() << "urls            = [" << vector_join(o.urls) << "]";
  Log::debug() << "output          = " << (o.outputs.empty() ? "(auto)" : "[" + vector_join(o.outputs) + "]");
  Log::debug() << "dir             = " << o.dir;
  Log::debug() << "threads_num     = " << o.threads_num;
  Log::debug() << "continue        = " << (o.continue_download ? "true" : "false");
  Log::debug() << "limit_rate      = " << (o.limit_rate.empty() ? "(none)" : o.limit_rate);
  Log::debug() << "retry           = " << o.retry;
  Log::debug() << "timeout         = " << o.timeout << "s";
  Log::debug() << "proxy           = " << (o.proxy.empty() ? "(none)" : o.proxy);
  Log::debug() << "headers         = [" << vector_join(o.headers) << "]";
  Log::debug() << "user_agent      = " << (o.user_agent.empty() ? "(default)" : o.user_agent);
  Log::debug() << "checksums        = " << (o.checksums.empty() ? "(none)" : "[" + vector_join(o.checksums) + "]");
  Log::debug() << "silent          = " << (o.silent ? "true" : "false");
  Log::debug() << "log_file        = " << (o.log_file.empty() ? "(console only)" : o.log_file);
  Log::debug() << "===================";
}

struct CurlGlobal {
  CurlGlobal()  { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlGlobal() { curl_global_cleanup(); }
};

int main(int argc, char **argv) {
  CurlGlobal curl_guard_;

  std::optional<BSD_CORE::Options> parsed;

  try{
    parsed = BSD_CORE::parse_options(argc, argv);
  }catch(const std::exception& e){
    BSD_CORE::Log::error() << "exception: " << e.what();
    return 1;
  }

  if (!parsed) return 0;

  BSD_CORE::Options opts = std::move(*parsed);

  BSD_CORE::Log::set_silent(opts.silent);
  if (!opts.log_file.empty()) {
    BSD_CORE::Log::set_file(opts.log_file);
  }

  BSD_CORE::Log::set_level(BSD_CORE::LogLevel::Info);

  dump_options(opts);

  BSD_CORE::ThreadPool tp(opts.threads_num);
  std::size_t failed = 0;

  if(!opts.checksums.empty() && opts.checksums.size() != opts.urls.size()){
    BSD_CORE::Log::error() << "num of hash check list is not as same as url list, invaild hash check request";
    BSD_CORE::Log::warn() << "clear the invaild check list";
    opts.checksums.clear();
  }

  if(!opts.outputs.empty() && opts.outputs.size() != opts.urls.size()){
    BSD_CORE::Log::error() << "num of hash output list is not as same as url list, invaild output request";
    BSD_CORE::Log::warn() << "clear the invaild output list";
    opts.outputs.clear();
  }

  for (std::size_t i = 0; i < opts.urls.size(); ++i) {
    const std::string& url = opts.urls[i];

    std::string checksum = "NO_HASH_CHECK";
    std::string output = "DEFAULT";
    if (!opts.checksums.empty()) checksum = opts.checksums[i];
    if (!opts.outputs.empty()) output = opts.outputs[i];

    const int max_attempts = opts.retry;
    bool ok = false;

    for (int attempt = 0; attempt < max_attempts + 1; ++attempt) {
      try {
        BSD_CORE::Downloader dl(tp, opts, url, checksum, output);
        dl.run();
        ok = true;
        BSD_CORE::Log::info() << "download finished: " << url;
        break;
      }
      catch (const BSD_CORE::RetryableError& e) {
        BSD_CORE::Log::warn()
          << "attempt " << (attempt + 1) << "/" << max_attempts
          << " failed: " << e.what();

        if (attempt + 1 < max_attempts) {
          auto backoff_ms = (1 << attempt) * 1000 + (std::rand() % 500);
          std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
        }
      }
      catch (const std::exception& e) {
        BSD_CORE::Log::error()
          << "download failed (not retryable): " << url
          << " : " << e.what();
        break;
      }
    }

    if(!ok) { ++failed; BSD_CORE::Log::error() << "download gave up: " << url; }

  }

  BSD_CORE::Log::info() << "all done";
  return failed == 0 ? 0 : 1;
}