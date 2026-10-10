#include <iostream>
#include "CLI/CLI.hpp"
#include "core.hpp"
#include "utils.hpp"
#include "option.hpp"

int main(int argc, char **argv) {
  CLI::App app{"BSD bishop9910 downloader - a multi-thread downloader"};

  app.set_version_flag("--version", BSD_CORE::versionToString(BSD_CORE::version));
  Options opts;

  app.add_option("url", opts.urls, "Download URL")->required();

  app.add_option("-o,--output", opts.output, "Output file name");
  app.add_option("-d,--dir", opts.dir, "Download directory")->default_val(".");
  app.add_option("-t,--threads", opts.threads_num, "Number of concurrent threads of each file")->default_val(16);
  app.add_flag("-c,--continue", opts.continue_download, "Resume download");
  app.add_option("--limit-rate", opts.limit_rate, "Rate limit per thread, e.g. 1M");
  app.add_option("--retry", opts.retry, "Number of retries")->default_val(3);
  app.add_option("--timeout", opts.timeout, "Timeout in seconds")->default_val(30);
  app.add_option("--proxy", opts.proxy, "Proxy URL");
  app.add_option("-H,--header", opts.headers, "Custom header, can be repeated")->take_all();
  app.add_option("--user-agent", opts.user_agent, "User-Agent");
  app.add_option("--checksum", opts.checksum, "Checksum, format type:hash");

  CLI11_PARSE(app, argc, argv);

  return 0;
}