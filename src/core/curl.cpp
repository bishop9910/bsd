#include "curl.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <curl/curl.h>

namespace BSD_CORE {

bool CaseInsensitiveLess::operator()(const std::string& a, const std::string& b) const {
  return std::lexicographical_compare(
    a.begin(), a.end(), b.begin(), b.end(),
    [](unsigned char x, unsigned char y) {
      return std::tolower(x) < std::tolower(y);
    });
}

std::uint64_t ResponseHead::total_bytes() const {
  if (content_length.empty()) return 0;
  try {
    std::size_t pos = 0;
    std::uint64_t v = std::stoull(content_length, &pos);
    return pos == content_length.size() ? v : 0;
  } catch (...) {
    return 0;
  }
}

bool ResponseHead::supports_range() const {
  std::string lower = accept_ranges;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return lower.find("bytes") != std::string::npos;
}

namespace {

std::string trim_ws(const std::string& s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  auto b = std::find_if(s.begin(), s.end(), not_space);
  auto e = std::find_if(s.rbegin(), s.rend(), not_space).base();
  return (b < e) ? std::string(b, e) : std::string{};
}

struct HeaderContext {
  ResponseHead head;
};

struct WriteContext {
  const SinkFn* sink = nullptr;
  std::uint64_t received = 0;
};

std::size_t header_cb(char* buffer, std::size_t size, std::size_t nitems, void* userdata) {
  auto* ctx = static_cast<HeaderContext*>(userdata);
  std::size_t len = size * nitems;
  std::string line(buffer, len);

  if (line.rfind("HTTP/", 0) == 0) {
    auto sp = line.find(' ');
    if (sp != std::string::npos) {
      long status = 0;
      try { status = std::stol(line.substr(sp + 1)); } catch (...) {}
      ctx->head = ResponseHead{};
      ctx->head.status = status;
    }
    return len;
  }

  if (line == "\r\n" || line == "\n") return len;

  auto colon = line.find(':');
  if (colon == std::string::npos) return len;

  std::string key   = trim_ws(line.substr(0, colon));
  std::string value = trim_ws(line.substr(colon + 1));

  if      (key == "Content-Type")        ctx->head.content_type        = value;
  else if (key == "Content-Length")      ctx->head.content_length      = value;
  else if (key == "Content-Disposition") ctx->head.content_disposition = value;
  else if (key == "Content-Range")       ctx->head.content_range       = value;
  else if (key == "ETag")                ctx->head.etag                = value;
  else if (key == "Last-Modified")       ctx->head.last_modified       = value;
  else if (key == "Accept-Ranges")       ctx->head.accept_ranges       = value;
  else if (key == "Location")            ctx->head.location            = value;
  else                                   ctx->head.extra[key]          = value;

  return len;
}

std::size_t write_cb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  auto* ctx = static_cast<WriteContext*>(userdata);
  std::size_t n = size * nmemb;
  if (ctx->sink && !(*ctx->sink)(ptr, n)) return 0;
  ctx->received += n;
  return n;
}

void apply_common(CURL* h, const CurlConfig& cfg) {
  curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(h, CURLOPT_MAXREDIRS, 10L);
  curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(h, CURLOPT_TIMEOUT, static_cast<long>(cfg.timeout));
  curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(h, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, cfg.verify_ssl ? 1L : 0L);
  curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, cfg.verify_ssl ? 2L : 0L);

  std::string ua = cfg.user_agent.empty() ? "bsd-downloader/1.0" : cfg.user_agent;
  curl_easy_setopt(h, CURLOPT_USERAGENT, ua.c_str());

  if (!cfg.proxy.empty())
    curl_easy_setopt(h, CURLOPT_PROXY, cfg.proxy.c_str());

  if (cfg.limit_rate > 0)
    curl_easy_setopt(h, CURLOPT_MAX_RECV_SPEED_LARGE,
                     static_cast<curl_off_t>(cfg.limit_rate));
}

struct curl_slist* build_headers(const CurlConfig& cfg,
                                 const std::vector<std::string>& extra) {
  struct curl_slist* headers = nullptr;
  for (const auto& h : cfg.headers) headers = curl_slist_append(headers, h.c_str());
  for (const auto& h : extra)       headers = curl_slist_append(headers, h.c_str());
  return headers;
}

} // namespace

void Curl::global_init() { curl_global_init(CURL_GLOBAL_DEFAULT); }
void Curl::global_cleanup() { curl_global_cleanup(); }

ResponseHead Curl::head(const std::string& url, const CurlConfig& cfg) {
  CURL* h = curl_easy_init();
  if (!h) throw std::runtime_error("curl_easy_init failed");

  HeaderContext hctx;
  char errbuf[CURL_ERROR_SIZE] = {0};

  curl_easy_setopt(h, CURLOPT_URL, url.c_str());
  curl_easy_setopt(h, CURLOPT_NOBODY, 1L);
  apply_common(h, cfg);
  curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, header_cb);
  curl_easy_setopt(h, CURLOPT_HEADERDATA, &hctx);
  curl_easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);

  auto* headers = build_headers(cfg, {});
  if (headers) curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);

  CURLcode rc = curl_easy_perform(h);

  char* eff = nullptr;
  curl_easy_getinfo(h, CURLINFO_EFFECTIVE_URL, &eff);
  if (eff) hctx.head.url = eff;

  if (headers) curl_slist_free_all(headers);
  curl_easy_cleanup(h);

  if (rc != CURLE_OK)
    throw std::runtime_error(std::string("curl HEAD: ") +
                             (errbuf[0] ? errbuf : curl_easy_strerror(rc)));

  return hctx.head;
}

RangeResult Curl::get_range(const std::string& url,
                            const CurlConfig& cfg,
                            std::uint64_t start,
                            std::uint64_t end,
                            const std::string& if_range,
                            const SinkFn& sink)
{
  CURL* h = curl_easy_init();
  if (!h) throw std::runtime_error("curl_easy_init failed");

  HeaderContext hctx;
  WriteContext  wctx;
  wctx.sink = &sink;

  char errbuf[CURL_ERROR_SIZE] = {0};

  curl_easy_setopt(h, CURLOPT_URL, url.c_str());
  curl_easy_setopt(h, CURLOPT_NOBODY, 0L);
  apply_common(h, cfg);
  curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, header_cb);
  curl_easy_setopt(h, CURLOPT_HEADERDATA, &hctx);
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(h, CURLOPT_WRITEDATA, &wctx);
  curl_easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);

  std::vector<std::string> extra;
  extra.push_back("Range: bytes=" + std::to_string(start) + "-" + std::to_string(end));
  if (!if_range.empty())
    extra.push_back("If-Range: " + if_range);

  auto* headers = build_headers(cfg, extra);
  if (headers) curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);

  CURLcode rc = curl_easy_perform(h);

  long status = 0;
  curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);

  if (headers) curl_slist_free_all(headers);
  curl_easy_cleanup(h);

  if (rc != CURLE_OK && rc != CURLE_WRITE_ERROR)
    throw std::runtime_error(std::string("curl GET: ") +
                             (errbuf[0] ? errbuf : curl_easy_strerror(rc)));

  RangeResult r;
  r.status        = status;
  r.received      = wctx.received;
  r.content_range = hctx.head.content_range;
  return r;
}

RangeResult Curl::get_range_to_file(const std::string& url,
                                    const CurlConfig& cfg,
                                    std::uint64_t start,
                                    std::uint64_t end,
                                    const std::string& if_range,
                                    std::ofstream& ofs)
{
  ofs.seekp(static_cast<std::streamoff>(start));
  if (!ofs.good()) throw std::runtime_error("seekp failed");

  SinkFn sink = [&ofs](const char* data, std::size_t len) -> bool {
    ofs.write(data, static_cast<std::streamsize>(len));
    return ofs.good();
  };

  return get_range(url, cfg, start, end, if_range, sink);
}

} // namespace BSD_CORE