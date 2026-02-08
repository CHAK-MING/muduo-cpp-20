#include "muduo/base/Logging.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/TimeZone.h"

#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

using namespace muduo;

namespace {

thread_local std::array<char, 512> t_errnobuf{};
thread_local std::array<char, 64> t_time{};
thread_local time_t t_lastSecond;

constexpr auto makeDigitPairs() {
  std::array<std::array<char, 2>, 100> pairs{};
  for (int i = 0; i < 100; ++i) {
    pairs.at(static_cast<size_t>(i)).at(0) = static_cast<char>('0' + (i / 10));
    pairs.at(static_cast<size_t>(i)).at(1) = static_cast<char>('0' + (i % 10));
  }
  return pairs;
}

constexpr auto kDigitPairs = makeDigitPairs();

inline void write2Digits(char *out, int value) {
  const auto &pair = kDigitPairs.at(static_cast<size_t>(value));
  out[0] = pair[0];
  out[1] = pair[1];
}

inline void write4Digits(char *out, int value) {
  if (value < 0) {
    value = 0;
  } else if (value > 9999) {
    value = 9999;
  }
  write2Digits(out, value / 100);
  write2Digits(out + 2, value % 100);
}

const char *cachedBasename(const char *file) {
  struct Cache {
    const char *file = nullptr;
    const char *basename = nullptr;
  };
  thread_local Cache cache{};

  if (file == nullptr) {
    return "unknown";
  }
  if (file == cache.file && cache.basename != nullptr) {
    return cache.basename;
  }

  const char *base = file;
  if (const char *slash = std::strrchr(file, '/'); slash != nullptr) {
    base = slash + 1;
  }
  cache.file = file;
  cache.basename = base;
  return base;
}

const char *errnoFallback(int savedErrno) {
  constexpr std::string_view kPrefix = "errno ";
  std::memcpy(t_errnobuf.data(), kPrefix.data(), kPrefix.size());
  auto *const toCharsBegin =
      t_errnobuf.data() + static_cast<std::ptrdiff_t>(kPrefix.size());
  auto *const toCharsEnd = t_errnobuf.data() + t_errnobuf.size() - 1;
  auto [ptr, ec] = std::to_chars(toCharsBegin, toCharsEnd, savedErrno);
  if (ec != std::errc{}) {
    return "errno ?";
  }
  *ptr = '\0';
  return t_errnobuf.data();
}

Logger::LogLevel initLogLevel() {
  if (::getenv("MUDUO_LOG_TRACE") != nullptr) {
    return Logger::LogLevel::TRACE;
  }
  if (::getenv("MUDUO_LOG_DEBUG") != nullptr) {
    return Logger::LogLevel::DEBUG;
  }
  return Logger::LogLevel::INFO;
}

const std::array<const char *,
                 static_cast<int>(Logger::LogLevel::NUM_LOG_LEVELS)>
    kLogLevelName = {"TRACE ", "DEBUG ", "INFO  ",
                     "WARN  ", "ERROR ", "FATAL "};

void defaultOutput(const char *msg, int len) {
  const size_t n = std::fwrite(msg, 1, static_cast<size_t>(len), stdout);
  (void)n;
}

void defaultFlush() { std::fflush(stdout); }

Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;
TimeZone g_logTimeZone;

} // namespace

namespace muduo {

Logger::LogLevel g_logLevel = initLogLevel();

const char *strerror_tl(int savedErrno) {
  const auto result =
      ::strerror_r(savedErrno, t_errnobuf.data(), t_errnobuf.size());
  if constexpr (std::is_same_v<decltype(result), int>) {
    return result == nullptr ? t_errnobuf.data() : errnoFallback(savedErrno);
  } else {
    return result != nullptr ? result : errnoFallback(savedErrno);
  }
}

Logger::Impl::Impl(LogLevel level, int savedErrno, std::string_view func,
                   std::source_location loc)
    : level_(level), loc_(loc), basename_(cachedBasename(loc.file_name())) {

  const auto nowTp = std::chrono::time_point_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now());
  time_ = nowTp;
  const auto microsSinceEpoch = nowTp.time_since_epoch().count();
  const auto seconds = static_cast<time_t>(microsSinceEpoch / 1000000);
  int us = static_cast<int>(microsSinceEpoch % 1000000);

  if (seconds != t_lastSecond) {
    t_lastSecond = seconds;
    DateTime dt = g_logTimeZone.valid() ? g_logTimeZone.toLocalTime(seconds)
                                        : TimeZone::toUtcTime(seconds);
    write4Digits(t_time.data(), dt.year);
    write2Digits(t_time.data() + 4, dt.month);
    write2Digits(t_time.data() + 6, dt.day);
    t_time.at(8) = ' ';
    write2Digits(t_time.data() + 9, dt.hour);
    t_time.at(11) = ':';
    write2Digits(t_time.data() + 12, dt.minute);
    t_time.at(14) = ':';
    write2Digits(t_time.data() + 15, dt.second);
    t_time.at(17) = '\0';
  }

  stream_ << std::string_view(t_time.data(), 17);

  std::array<char, 9> usbuf{};
  usbuf.at(0) = '.';
  const int hi = us / 10000;
  const int mid = (us / 100) % 100;
  const int lo = us % 100;
  usbuf.at(1) = kDigitPairs.at(static_cast<size_t>(hi)).at(0);
  usbuf.at(2) = kDigitPairs.at(static_cast<size_t>(hi)).at(1);
  usbuf.at(3) = kDigitPairs.at(static_cast<size_t>(mid)).at(0);
  usbuf.at(4) = kDigitPairs.at(static_cast<size_t>(mid)).at(1);
  usbuf.at(5) = kDigitPairs.at(static_cast<size_t>(lo)).at(0);
  usbuf.at(6) = kDigitPairs.at(static_cast<size_t>(lo)).at(1);

  if (g_logTimeZone.valid()) {
    usbuf.at(7) = ' ';
    stream_.append(usbuf.data(), 8);
  } else {
    usbuf.at(7) = 'Z';
    usbuf.at(8) = ' ';
    stream_.append(usbuf.data(), 9);
  }

  [[maybe_unused]] const int tid = CurrentThread::tid();
  stream_ << std::string_view(
      CurrentThread::tidString(),
      static_cast<size_t>(CurrentThread::tidStringLength()));
  stream_ << kLogLevelName.at(static_cast<int>(level));
  if (!func.empty()) {
    stream_ << func << ' ';
  }
  if (savedErrno != 0) {
    stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
  }
}

void Logger::Impl::finish() {
  struct SuffixCache {
    const char *basename = nullptr;
    std::uint_least32_t line = 0;
    std::array<char, 256> suffix{};
    size_t length = 0;
  };
  thread_local SuffixCache cache{};

  const auto line = loc_.line();
  if (cache.basename != basename_ || cache.line != line) {
    size_t pos = 0;
    constexpr std::string_view kPrefix = " - ";
    const auto appendRaw = [&pos](const char *data, size_t len) {
      if (pos >= cache.suffix.size()) {
        return;
      }
      const size_t avail = cache.suffix.size() - pos;
      const size_t n = len < avail ? len : avail;
      std::memcpy(cache.suffix.data() + static_cast<std::ptrdiff_t>(pos), data,
                  n);
      pos += n;
    };
    const auto appendChar = [&pos](char c) {
      if (pos < cache.suffix.size()) {
        cache.suffix.at(pos++) = c;
      }
    };

    appendRaw(kPrefix.data(), kPrefix.size());

    const size_t basenameLen = std::strlen(basename_);
    appendRaw(basename_, basenameLen);
    appendChar(':');

    if (pos < cache.suffix.size()) {
      auto [ptr, ec] =
          std::to_chars(cache.suffix.data() + static_cast<std::ptrdiff_t>(pos),
                        cache.suffix.data() + cache.suffix.size(),
                        static_cast<unsigned>(line));
      if (ec == std::errc{}) {
        pos += static_cast<size_t>(ptr - (cache.suffix.data() + pos));
      } else {
        appendChar('?');
      }
    } else {
      appendChar('?');
    }
    appendChar('\n');

    cache.basename = basename_;
    cache.line = line;
    cache.length = pos;
  }

  stream_.append(std::string_view(cache.suffix.data(), cache.length));
}

Logger::Logger(LogLevel level, int savedErrno, std::string_view func,
               std::source_location loc)
    : impl_(level, savedErrno, func, loc) {}

Logger::~Logger() {
  impl_.finish();
  const LogStream::Buffer &buf(stream().buffer());
  g_output(buf.data(), buf.length());
  if (impl_.level_ == Logger::LogLevel::FATAL) {
    g_flush();
    std::abort();
  }
}

Logger::LogLevel Logger::logLevel() { return g_logLevel; }

void Logger::setLogLevel(LogLevel level) { g_logLevel = level; }

void Logger::setOutput(OutputFunc out) { g_output = out; }

void Logger::setFlush(FlushFunc flush) { g_flush = flush; }

void Logger::setTimeZone(const TimeZone &tz) { g_logTimeZone = tz; }

} // namespace muduo
