#pragma once

#include "muduo/base/LogStream.h"

#include <cstdint>
#include <chrono>
#include <cerrno>
#include <optional>
#include <source_location>
#include <string_view>
#include <utility>

namespace muduo {

class TimeZone;

class Logger {
public:
  enum class LogLevel : std::uint8_t {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    NUM_LOG_LEVELS,
  };

  explicit Logger(LogLevel level = LogLevel::INFO, int savedErrno = 0,
                  std::string_view func = {},
                  std::source_location loc = std::source_location::current());
  ~Logger();

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  [[nodiscard]] LogStream &stream() { return impl_.stream_; }

  [[nodiscard]] static LogLevel logLevel();
  static void setLogLevel(LogLevel level);

  using OutputFunc = void (*)(const char *msg, int len);
  using FlushFunc = void (*)();
  static void setOutput(OutputFunc);
  static void setFlush(FlushFunc);
  static void setTimeZone(const TimeZone &tz);

private:
  class Impl {
  public:
    Impl(LogLevel level, int savedErrno, std::string_view func,
         std::source_location loc);

    void finish();

    std::chrono::sys_time<std::chrono::microseconds> time_;
    LogStream stream_;
    LogLevel level_;
    std::source_location loc_;
    const char *basename_;
  };

  Impl impl_;
};

extern Logger::LogLevel g_logLevel;

[[nodiscard]] const char *strerror_tl(int savedErrno);

template <typename... Args>
inline void logFmt(Logger::LogLevel level, std::source_location loc,
                   std::format_string<Args...> fmt, Args &&...args) {
  if (Logger::logLevel() <= level) {
    Logger(level, 0, {}, loc).stream().format(fmt, std::forward<Args>(args)...);
  }
}

template <typename... Args>
inline void logFmtErr(Logger::LogLevel level, int savedErrno,
                      std::source_location loc, std::format_string<Args...> fmt,
                      Args &&...args) {
  if (Logger::logLevel() <= level) {
    auto &s = Logger(level, savedErrno, {}, loc).stream();
    s.format(fmt, std::forward<Args>(args)...);
  }
}

[[nodiscard]] inline bool shouldLog(Logger::LogLevel level) {
  return level >= Logger::LogLevel::WARN || Logger::logLevel() <= level;
}

class LogLine {
public:
  explicit LogLine(Logger::LogLevel level, int savedErrno = 0,
                   std::string_view func = {},
                   std::source_location loc = std::source_location::current())
      : enabled_(shouldLog(level)) {
    if (enabled_) {
      logger_.emplace(level, savedErrno, func, loc);
    }
  }

  template <typename T> LogLine &operator<<(T &&value) {
    if (enabled_) {
      logger_->stream() << std::forward<T>(value);
    }
    return *this;
  }

  template <typename... Args>
  LogLine &format(std::format_string<Args...> fmt, Args &&...args) {
    if (enabled_) {
      logger_->stream().format(fmt, std::forward<Args>(args)...);
    }
    return *this;
  }

private:
  bool enabled_;
  std::optional<Logger> logger_;
};

[[nodiscard]] inline LogLine
logTrace(std::source_location loc = std::source_location::current()) {
  return LogLine(Logger::LogLevel::TRACE, 0, loc.function_name(), loc);
}

[[nodiscard]] inline LogLine
logDebug(std::source_location loc = std::source_location::current()) {
  return LogLine(Logger::LogLevel::DEBUG, 0, loc.function_name(), loc);
}

[[nodiscard]] inline LogLine
logInfo(std::source_location loc = std::source_location::current()) {
  return LogLine(Logger::LogLevel::INFO, 0, {}, loc);
}

[[nodiscard]] inline LogLine
logWarn(std::source_location loc = std::source_location::current()) {
  return LogLine(Logger::LogLevel::WARN, 0, {}, loc);
}

[[nodiscard]] inline LogLine
logError(std::source_location loc = std::source_location::current()) {
  return LogLine(Logger::LogLevel::ERROR, 0, {}, loc);
}

[[nodiscard]] inline LogLine
logFatal(std::source_location loc = std::source_location::current()) {
  return LogLine(Logger::LogLevel::FATAL, 0, {}, loc);
}

[[nodiscard]] inline LogLine
logSysErr(std::source_location loc = std::source_location::current()) {
  return LogLine(Logger::LogLevel::ERROR, errno, {}, loc);
}

[[nodiscard]] inline LogLine
logSysFatal(std::source_location loc = std::source_location::current()) {
  return LogLine(Logger::LogLevel::FATAL, errno, {}, loc);
}

template <typename... Args>
inline void logTrace(std::format_string<Args...> fmt, Args &&...args) {
  if (Logger::logLevel() <= Logger::LogLevel::TRACE) {
    logFmt(Logger::LogLevel::TRACE, std::source_location::current(), fmt,
           std::forward<Args>(args)...);
  }
}

template <typename... Args>
inline void logDebug(std::format_string<Args...> fmt, Args &&...args) {
  if (Logger::logLevel() <= Logger::LogLevel::DEBUG) {
    logFmt(Logger::LogLevel::DEBUG, std::source_location::current(), fmt,
           std::forward<Args>(args)...);
  }
}

template <typename... Args>
inline void logInfo(std::format_string<Args...> fmt, Args &&...args) {
  if (Logger::logLevel() <= Logger::LogLevel::INFO) {
    logFmt(Logger::LogLevel::INFO, std::source_location::current(), fmt,
           std::forward<Args>(args)...);
  }
}

template <typename... Args>
inline void logWarn(std::format_string<Args...> fmt, Args &&...args) {
  logFmt(Logger::LogLevel::WARN, std::source_location::current(), fmt,
         std::forward<Args>(args)...);
}

template <typename... Args>
inline void logError(std::format_string<Args...> fmt, Args &&...args) {
  logFmt(Logger::LogLevel::ERROR, std::source_location::current(), fmt,
         std::forward<Args>(args)...);
}

template <typename... Args>
inline void logFatal(std::format_string<Args...> fmt, Args &&...args) {
  logFmt(Logger::LogLevel::FATAL, std::source_location::current(), fmt,
         std::forward<Args>(args)...);
}

template <typename... Args>
inline void logSysErr(std::format_string<Args...> fmt, Args &&...args) {
  logFmtErr(Logger::LogLevel::ERROR, errno, std::source_location::current(), fmt,
            std::forward<Args>(args)...);
}

template <typename... Args>
inline void logSysFatal(std::format_string<Args...> fmt, Args &&...args) {
  logFmtErr(Logger::LogLevel::FATAL, errno, std::source_location::current(), fmt,
            std::forward<Args>(args)...);
}

template <typename T>
T *CheckNotNull(const char *names, T *ptr,
                std::source_location loc = std::source_location::current()) {
  if (ptr == nullptr) {
    Logger(Logger::LogLevel::FATAL, 0, {}, loc).stream() << names;
  }
  return ptr;
}

} // namespace muduo
