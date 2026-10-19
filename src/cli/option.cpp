#include <optional>
#include <stdexcept>
#include <string>
#include <cstdint>

#include <CLI/CLI.hpp>
#include "bsd/bsd_core.h"
#include "option.hpp"

namespace {

  // Small, dependency free copy of the size parser: the CLI no longer links
  // the C++ internals of the core, only the exported C API.
  std::uint64_t parse_size(const std::string& s) {
    if (s.empty()) return 0;
    std::uint64_t mul = 1;
    std::string num = s;
    char suffix = s.back();
    if (suffix == 'K' || suffix == 'k') { mul = 1024ULL;                 num.pop_back(); }
    else if (suffix == 'M' || suffix == 'm') { mul = 1024ULL * 1024;     num.pop_back(); }
    else if (suffix == 'G' || suffix == 'g') { mul = 1024ULL * 1024 * 1024; num.pop_back(); }

    try {
      std::size_t pos = 0;
      std::uint64_t v = std::stoull(num, &pos);
      if (pos != num.size()) throw std::invalid_argument("trailing");
      return v * mul;
    } catch (...) {
      throw std::runtime_error("invalid size: " + s);
    }
  }

} // namespace

namespace BSD_CORE {

  std::optional<Options> parse_options(int argc, char** argv) {
   CLI::App app{"BSD bishop9910 downloader - a multi-thread downloader"};

   app.set_version_flag("-v,--version", bsd_version());

   Options opts;

   // ---- URL参数 ----
   app.add_option("url", opts.urls, "Download URL(multiple)")
      ->required()
      ->expected(-1);   // 收 1 到 N 个

   // ---- 基础 ----
   app.add_option("-o,--output", opts.outputs, "Output file name(multiple)");
   app.add_option("-d,--dir",    opts.dir,    "Download directory")
      ->default_val(".");
   app.add_option("-t,--threads", opts.threads_num, "Number of concurrent threads")
      ->default_val(16)
      ->check(CLI::Range(1, 256));

   // ---- 行为 ----
   app.add_flag("-c,--continue", opts.continue_download, "Resume download");
   app.add_option("--limit-rate", opts.limit_rate, "Total rate limit for all threads, e.g. 1M")
      ->default_val("1G");
   app.add_option("--retry",   opts.retry,   "Number of retries")
      ->default_val(3);
   app.add_option("--timeout", opts.timeout, "Timeout in seconds")
      ->default_val(90);

   // ---- 网络 ----
   app.add_option("--proxy",      opts.proxy,      "Proxy URL");
   app.add_option("-H,--header",  opts.headers,    "Custom header, can be repeated")
      ->take_all(); // 全要
   app.add_option("--user-agent", opts.user_agent, "User-Agent");
   app.add_option("--checksum",   opts.checksums,   "Checksum, format type:hash(multiple)");

   // ---- 日志 ----
   app.add_flag("-s,--silent",      opts.silent,   "Silent mode, no output");
   app.add_option("--log-file",     opts.log_file, "Write log to file");

   try {
      app.parse(argc, argv);
      opts.limit_rate_bytes = parse_size(opts.limit_rate);
   } catch (const CLI::ParseError& e) {
      app.exit(e);
      return std::nullopt;
   }catch(const std::exception& e){
      const std::string message = std::string("exception: ") + e.what();
      bsd_log_message(BSD_LOG_ERROR, message.c_str());
      return std::nullopt;
   }

   return opts;
  }

} // namespace BSD_CORE
