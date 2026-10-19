#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include <fmt/color.h>
#include <fmt/format.h>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <io.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace BSD_CORE {

enum class LogLevel : int {
  Trace = 0,
  Debug,
  Info,
  Warn,
  Error,
  Off
};

// level is the numeric LogLevel value.
using LogSink = void (*)(int level, const char* message, void* user);

class Log {
public:
  static void set_silent(bool s)          { silent() = s; }
  static void set_console(bool on)        { console() = on; }
  static void set_level(LogLevel lv)      { min_level() = lv; }
  static void set_color(bool on)          { color() = on; }

  static void set_callback(LogSink cb, void* user) {
    std::lock_guard<std::mutex> lock(mutex());
    callback()      = cb;
    callback_user() = user;
  }

  static void set_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex());
    if (file().is_open()) file().close();
    file().clear();
    if (!path.empty()) file().open(path, std::ios::out | std::ios::app);
  }

  class Line {
  public:
    explicit Line(LogLevel level) : level_(level) {}
    ~Line() {
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
  static std::mutex&      mutex()         { static std::mutex m;         return m; }
  static std::ofstream&   file()          { static std::ofstream f;      return f; }
  static bool&            silent()        { static bool s = false;        return s; }
  static bool&            console()       { static bool c = true;         return c; }
  static bool&            color()         { static bool c = true;         return c; }
  static LogLevel&        min_level()     { static LogLevel l = LogLevel::Info; return l; }
  static LogSink&         callback()      { static LogSink cb = nullptr;   return cb; }
  static void*&           callback_user() { static void* u  = nullptr;    return u; }

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

  static fmt::text_style style_of(LogLevel lv) {
    switch (lv) {
      case LogLevel::Trace: return fmt::fg(fmt::color::gray);
      case LogLevel::Debug: return fmt::fg(fmt::color::cyan);
      case LogLevel::Info:  return fmt::fg(fmt::color::green);
      case LogLevel::Warn:  return fmt::fg(fmt::color::orange);
      case LogLevel::Error: return fmt::emphasis::bold | fmt::fg(fmt::color::red);
      default:              return {};
    }
  }

  static bool stdout_is_tty() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
  }

  static bool enable_vt() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return false;
    return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    return true;
#endif
  }

  static bool color_enabled() {
    static const bool vt_ok = [] {
      if (!stdout_is_tty()) return false;
      return enable_vt();
    }();
    return vt_ok && color();
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

    std::string ts = fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
                                 tm_buf.tm_year + 1900, tm_buf.tm_mon + 1,
                                 tm_buf.tm_mday, tm_buf.tm_hour,
                                 tm_buf.tm_min, tm_buf.tm_sec,
                                 static_cast<int>(ms.count()));

    LogSink cb     = nullptr;
    void*   cb_user = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex());

      if (console()) {
        if (color_enabled()) {
          fmt::print(style_of(level), "[{}] [{}] {}\n",
                     ts, level_str(level), msg);
        } else {
          fmt::print("[{}] [{}] {}\n", ts, level_str(level), msg);
        }
      }

      if (file().is_open()) {
        file() << '[' << ts << "] [" << level_str(level) << "] " << msg << '\n';
        file().flush();
      }

      cb      = callback();
      cb_user = callback_user();
    }

    // Called outside the lock so a client callback may log again.
    if (cb) cb(static_cast<int>(level), msg.c_str(), cb_user);
  }
};

} // namespace BSD_CORE