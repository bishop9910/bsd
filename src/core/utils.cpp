#include "utils.hpp"

namespace BSD_CORE {
  std::string versionToString(std::array<int, 3> version){
    std::string ver_s = "";
    for(int i=0; i<3; i++){
      ver_s.append(std::to_string(version[i]));
      if(i != 2){
        ver_s.append(".");
      }
    }
    return ver_s;
  }

  std::string trim(const std::string& s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    auto b = std::find_if(s.begin(), s.end(), not_space);
    auto e = std::find_if(s.rbegin(), s.rend(), not_space).base();
    return (b < e) ? std::string(b, e) : std::string{};
  }

  std::map<std::string, std::string, CaseInsensitiveLess> rawHeaderToMap(const std::vector<std::string>& raw_headers) {
    std::map<std::string, std::string, CaseInsensitiveLess> m;
    for (const auto& line : raw_headers) {
      auto colon = line.find(':');
      if (colon == std::string::npos) continue;   // 非法行，跳过
      std::string key   = trim(line.substr(0, colon));
      std::string value = trim(line.substr(colon + 1));
      if (key.empty()) continue;
      m[key] = value;
    }
    return m;
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

  uint64_t parse_size(const std::string& s) {
    if (s.empty()) return 0;
    uint64_t mul = 1;
    std::string num = s;
    char suffix = s.back();
    if (suffix == 'K' || suffix == 'k') { mul = 1024ULL;              num.pop_back(); }
    else if (suffix == 'M' || suffix == 'm') { mul = 1024ULL * 1024;  num.pop_back(); }
    else if (suffix == 'G' || suffix == 'g') { mul = 1024ULL*1024*1024; num.pop_back(); }

    try {
      size_t pos = 0;
      uint64_t v = std::stoull(num, &pos);
      if (pos != num.size()) throw std::invalid_argument("trailing");
      return v * mul;
    } catch (...) {
      throw std::runtime_error("invalid size: " + s);
    }
  }

  std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());

    auto hex = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };

    for (std::size_t i = 0; i < s.size(); ++i) {
      if (s[i] == '%' && i + 2 < s.size()) {
        int hi = hex(s[i + 1]);
        int lo = hex(s[i + 2]);
        if (hi >= 0 && lo >= 0) {
          out.push_back(static_cast<char>((hi << 4) | lo));
          i += 2;
          continue;
        }
      }
      if (s[i] == '+') {
        out.push_back(' ');
      } else {
        out.push_back(s[i]);
      }
    }
    return out;
  }

  std::string now_string() {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto t   = system_clock::to_time_t(now);
    auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
  #ifdef _WIN32
    localtime_s(&tm_buf, &t);
  #else
    localtime_r(&t, &tm_buf);
  #endif

    std::ostringstream os;
    os << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
      << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return os.str();
  }

  std::string http_time(std::time_t t) {
    std::tm tm_buf{};
  #ifdef _WIN32
    gmtime_s(&tm_buf, &t);
  #else
    gmtime_r(&t, &tm_buf);
  #endif

    std::ostringstream os;
    os << std::put_time(&tm_buf, "%a, %d %b %Y %H:%M:%S GMT");
    return os.str();
  }

  std::string http_time_now() {
    return http_time(std::time(nullptr));
  }

} // namespace BSD_CORE
