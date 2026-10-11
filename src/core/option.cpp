#include <optional>
#include <stdexcept>
#include <string>

#include <CLI/CLI.hpp>
#include "core.hpp"   // version
#include "log.hpp"
#include "utils.hpp"  // versionToString
#include "option.hpp"

namespace BSD_CORE {

  std::optional<Options> parse_options(int argc, char** argv) {
   CLI::App app{"BSD bishop9910 downloader - a multi-thread downloader"};

   app.set_version_flag("-v,--version", versionToString(version));

   Options opts;

   // ---- URL参数 ----
   app.add_option("url", opts.urls, "Download URL(Can be multiple)")
      ->required()
      ->expected(-1);   // 收 1 到 N 个

   // ---- 基础 ----
   app.add_option("-o,--output", opts.output, "Output file name");
   app.add_option("-d,--dir",    opts.dir,    "Download directory")
      ->default_val(".");
   app.add_option("-t,--threads", opts.threads_num, "Number of concurrent threads of each file")
      ->default_val(16)
      ->check(CLI::Range(1, 256));

   // ---- 行为 ----
   app.add_flag("-c,--continue", opts.continue_download, "Resume download");
   app.add_option("--limit-rate", opts.limit_rate, "Rate limit per thread, e.g. 1M")
      ->default_val("1G");
   app.add_option("--retry",   opts.retry,   "Number of retries")
      ->default_val(3);
   app.add_option("--timeout", opts.timeout, "Timeout in seconds")
      ->default_val(30);

   // ---- 网络 ----
   app.add_option("--proxy",      opts.proxy,      "Proxy URL");
   app.add_option("-H,--header",  opts.headers,    "Custom header, can be repeated")
      ->take_all();
   app.add_option("--user-agent", opts.user_agent, "User-Agent");
   app.add_option("--checksum",   opts.checksum,   "Checksum, format type:hash");

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
      Log::error() << "exception: " << e.what();
      return std::nullopt;
   }

   return opts;
  }

} // namespace BSD_CORE