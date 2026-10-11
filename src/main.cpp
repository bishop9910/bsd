#include <iostream>
#include "thread_pool.hpp"
#include "core.hpp"
#include "utils.hpp"
#include "option.hpp"
#include "log.hpp"
#include "download.hpp"
#include <curl/curl.h>

// 把 vector<string> 拼成 "a, b, c"
std::string vector_join(const std::vector<std::string>& v, const char* sep = ", ") {
  std::ostringstream os;
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (i) os << sep;
    os << v[i];
  }
  return os.str();
}

void dump_options(const BSD_CORE::Options& o) {
  using BSD_CORE::Log;

  Log::debug() << "===== Options =====";
  Log::debug() << "urls            = [" << vector_join(o.urls) << "]";
  Log::debug() << "output          = " << (o.output.empty() ? "(auto)" : o.output);
  Log::debug() << "dir             = " << o.dir;
  Log::debug() << "threads_num     = " << o.threads_num;
  Log::debug() << "continue        = " << (o.continue_download ? "true" : "false");
  Log::debug() << "limit_rate      = " << (o.limit_rate.empty() ? "(none)" : o.limit_rate);
  Log::debug() << "retry           = " << o.retry;
  Log::debug() << "timeout         = " << o.timeout << "s";
  Log::debug() << "proxy           = " << (o.proxy.empty() ? "(none)" : o.proxy);
  Log::debug() << "headers         = [" << vector_join(o.headers) << "]";
  Log::debug() << "user_agent      = " << (o.user_agent.empty() ? "(default)" : o.user_agent);
  Log::debug() << "checksum        = " << (o.checksum.empty() ? "(none)" : o.checksum);
  Log::debug() << "silent          = " << (o.silent ? "true" : "false");
  Log::debug() << "log_file        = " << (o.log_file.empty() ? "(console only)" : o.log_file);
  Log::debug() << "===================";
}

int main(int argc, char **argv) {

  curl_global_init(CURL_GLOBAL_DEFAULT);

  std::optional<BSD_CORE::Options> parsed;

  try{
    parsed = BSD_CORE::parse_options(argc, argv);
  }catch(const std::exception& e){
    BSD_CORE::Log::error() << "exception: " << e.what();
  }

  if (!parsed) return 0;

  BSD_CORE::Options opts = std::move(*parsed);

  BSD_CORE::Log::set_silent(opts.silent);
  if (!opts.log_file.empty()) {
    BSD_CORE::Log::set_file(opts.log_file);
  }

  BSD_CORE::Log::set_level(BSD_CORE::LogLevel::Debug);

  dump_options(opts);

  BSD_CORE::ThreadPool tp(opts.threads_num);

  for (auto& url : opts.urls) {
    BSD_CORE::Downloader dl(tp, opts, url);
    try {
      dl.run();
      BSD_CORE::Log::info() << "download finished: " << url;
    } catch (const std::exception& e) {
      BSD_CORE::Log::error() << "download failed: " << url << " exception: " << e.what();
    }
  }

  BSD_CORE::Log::info() << "all done";

  curl_global_cleanup();
  return 0;
}