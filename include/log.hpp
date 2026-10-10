#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace BSD_CORE {

enum class LogLevel : int {
  Trace = 0,
  Debug,
  Info,
  Warn,
  Error,
  Off
};

class Log {
public:
  // ---- 配置 ----
  static void set_silent(bool s)          { silent() = s; }
  static void set_console(bool on)        { console() = on; }
  static void set_level(LogLevel lv)      { min_level() = lv; }

  static void set_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex());
    if (file().is_open()) file().close();
    file().clear();
    if (!path.empty()) {
      file().open(path, std::ios::out | std::ios::app);
    }
  }

  // ---- 流式接口 ----
  class Line {
  public:
    explicit Line(LogLevel level) : level_(level) {}

    ~Line() {
      // silent 优先级最高；其次级别过滤
      if (silent()) return;
      if (min_level() == LogLevel::Off) return;
      if (level_ < min_level()) return;
      output(level_, os_.str());
    }

    Line(const Line&)            = delete;
    Line& operator=(const Line&) = delete;

    template <class T>
    Line& operator<<(const T& v) { os_ << v; return *this; }

  private:
    LogLevel level_;
    std::ostringstream os_;
  };

  static Line trace() { return Line(LogLevel::Trace); }
  static Line debug() { return Line(LogLevel::Debug); }
  static Line info()  { return Line(LogLevel::Info);  }
  static Line warn()  { return Line(LogLevel::Warn);  }
  static Line error() { return Line(LogLevel::Error); }

private:
  static std::mutex&      mutex()     { static std::mutex m;      return m; }
  static std::ofstream&   file()      { static std::ofstream f;   return f; }
  static bool&            silent()    { static bool s = false;     return s; }
  static bool&            console()   { static bool c = true;      return c; }
  static LogLevel&        min_level() { static LogLevel l = LogLevel::Info; return l; }

  static const char* level_str(LogLevel lv) {
    switch (lv) {
      case LogLevel::Trace: return "TRACE";
      case LogLevel::Debug: return "DEBUG";
      case LogLevel::Info:  return "INFO ";
      case LogLevel::Warn:  return "WARN ";
      case LogLevel::Error: return "ERROR";
      default:              return "?????";
    }
  }

  static void output(LogLevel level, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                 now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif

    std::ostringstream line;
    line << '[' << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
         << '.' << std::setfill('0') << std::setw(3) << ms.count() << "] "
         << '[' << level_str(level) << "] "
         << msg << '\n';

    std::lock_guard<std::mutex> lock(mutex());
    if (console()) std::cout << line.str();
    if (file().is_open()) {
      file() << line.str();
      file().flush();   // 想性能好可以去掉，崩溃时可能丢最后几行
    }
  }
};

} // namespace BSD_CORE