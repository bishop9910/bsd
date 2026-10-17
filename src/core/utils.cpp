#include "utils.hpp"

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__linux__)
#  include <unistd.h>
#  include <limits.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#endif

namespace BSD_CORE {
  namespace fs = std::filesystem;
  fs::path GetExePath();

  std::string versionToString(const std::vector<int>& version, const int& build_version, const char* sep){
    std::ostringstream ver_s;
    for(int i = 0; i < version.size(); i++){
      if(i) ver_s << sep;
      ver_s << version[i];
    }
    ver_s << "+" << build_version;
    if(build_version == 0){
      ver_s << "(beta)" << "-" << "[SHA256:" << compute_hash(GetExePath(), "sha256") << "]";
    }
    return ver_s.str();
  }

  std::string vector_join(const std::vector<std::string>& v, const char* sep) {
    std::ostringstream os;
    for (std::size_t i = 0; i < v.size(); ++i) {
      if (i) os << sep;
      os << v[i];
    }
    return os.str();
  }

  fs::path GetExePath() {
  #if defined(_WIN32)
    std::vector<wchar_t> buf(MAX_PATH);
    while (true) {
      DWORD len = ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
      if (len == 0) throw std::runtime_error("GetModuleFileNameW failed");
      if (len < buf.size()) return fs::path(std::wstring(buf.data(), len));
      buf.resize(buf.size() * 2);
    }
  #elif defined(__linux__)
    std::vector<char> buf(PATH_MAX);
    while (true) {
      ssize_t len = ::readlink("/proc/self/exe", buf.data(), buf.size());
      if (len < 0) throw std::runtime_error("readlink failed");
      if (static_cast<size_t>(len) < buf.size())
        return fs::path(std::string(buf.data(), len));
      buf.resize(buf.size() * 2);
    }
  #elif defined(__APPLE__)
    uint32_t size = 0;
    ::_NSGetExecutablePath(nullptr, &size);  // 先问需要多大
    std::vector<char> buf(size);
    if (::_NSGetExecutablePath(buf.data(), &size) != 0)
      throw std::runtime_error("_NSGetExecutablePath failed");
    return fs::canonical(buf.data());  // 解析符号链接/相对路径
  #else
  #  error "Unsupported platform"
  #endif
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

  std::filesystem::path PathFromUtf8(const std::string& s) {
    return std::filesystem::path(reinterpret_cast<const char8_t*>(s.c_str()));
  }

} // namespace BSD_CORE
