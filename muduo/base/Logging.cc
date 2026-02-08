#include "muduo/base/Logging.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/TimeZone.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>

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
#if ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) &&                   \
     !defined(_GNU_SOURCE))
  if (::strerror_r(savedErrno, t_errnobuf.data(), t_errnobuf.size()) == 0) {
    return t_errnobuf.data();
  }
  return errnoFallback(savedErrno);
#else
  const char *msg =
      ::strerror_r(savedErrno, t_errnobuf.data(), t_errnobuf.size());
  if (msg != nullptr) {
    return msg;
  }
  return errnoFallback(savedErrno);
#endif
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
    auto out = std::format_to_n(
        t_time.data(), static_cast<std::ptrdiff_t>(t_time.size() - 1),
        "{:04}{:02}{:02} {:02}:{:02}:{:02}", dt.year, dt.month, dt.day, dt.hour,
        dt.minute, dt.second);
    const size_t written =
        std::min(static_cast<size_t>(out.size), t_time.size() - 1);
    t_time.at(written) = '\0';
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
  using namespace std::string_view_literals;
  stream_ << " - "sv << std::string_view(basename_) << ':'
          << static_cast<int>(loc_.line()) << '\n';
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
