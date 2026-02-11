#pragma once

#include "muduo/base/CxxFeatures.h"

#include <cstdio>
#include <format>
#include <string_view>
#include <utility>
#if MUDUO_HAS_CPP23_PRINT
#include <print>
#endif

namespace muduo::io {

namespace detail {

inline void writeFile(std::FILE *stream, std::string_view text) {
  if (stream == nullptr || text.empty()) {
    return;
  }
  const auto n = std::fwrite(text.data(), 1, text.size(), stream);
  (void)n;
}

inline void writeLineFile(std::FILE *stream, std::string_view text) {
  writeFile(stream, text);
  writeFile(stream, "\n");
}

} // namespace detail

inline void flush() { std::fflush(stdout); }

template <typename... Args>
inline void print(std::format_string<Args...> fmt, Args &&...args) {
#if MUDUO_HAS_CPP23_PRINT
  std::print(fmt, std::forward<Args>(args)...);
#else
  detail::writeFile(stdout, std::format(fmt, std::forward<Args>(args)...));
#endif
}

template <typename... Args>
inline void println(std::format_string<Args...> fmt, Args &&...args) {
#if MUDUO_HAS_CPP23_PRINT
  std::println(fmt, std::forward<Args>(args)...);
#else
  detail::writeLineFile(stdout, std::format(fmt, std::forward<Args>(args)...));
#endif
}

inline void print(std::string_view text) {
#if MUDUO_HAS_CPP23_PRINT
  std::print("{}", text);
#else
  detail::writeFile(stdout, text);
#endif
}

inline void println(std::string_view text) {
#if MUDUO_HAS_CPP23_PRINT
  std::println("{}", text);
#else
  detail::writeLineFile(stdout, text);
#endif
}

inline void println() { println(std::string_view{}); }

template <typename... Args>
inline void eprint(std::format_string<Args...> fmt, Args &&...args) {
#if MUDUO_HAS_CPP23_PRINT
  std::print(stderr, fmt, std::forward<Args>(args)...);
#else
  detail::writeFile(stderr, std::format(fmt, std::forward<Args>(args)...));
#endif
}

template <typename... Args>
inline void eprintln(std::format_string<Args...> fmt, Args &&...args) {
#if MUDUO_HAS_CPP23_PRINT
  std::println(stderr, fmt, std::forward<Args>(args)...);
#else
  detail::writeLineFile(stderr, std::format(fmt, std::forward<Args>(args)...));
#endif
}

inline void eprint(std::string_view text) {
#if MUDUO_HAS_CPP23_PRINT
  std::print(stderr, "{}", text);
#else
  detail::writeFile(stderr, text);
#endif
}

inline void eprintln(std::string_view text) {
#if MUDUO_HAS_CPP23_PRINT
  std::println(stderr, "{}", text);
#else
  detail::writeLineFile(stderr, text);
#endif
}

inline void eflush() { std::fflush(stderr); }

#if MUDUO_ENABLE_LEGACY_COMPAT
inline void print(std::FILE *stream, std::string_view text) {
  detail::writeFile(stream, text);
}

inline void println(std::FILE *stream, std::string_view text) {
  detail::writeLineFile(stream, text);
}

template <typename... Args>
inline void print(std::FILE *stream, std::format_string<Args...> fmt,
                  Args &&...args) {
#if MUDUO_HAS_CPP23_PRINT
  std::print(stream, fmt, std::forward<Args>(args)...);
#else
  print(stream, std::format(fmt, std::forward<Args>(args)...));
#endif
}

template <typename... Args>
inline void println(std::FILE *stream, std::format_string<Args...> fmt,
                    Args &&...args) {
#if MUDUO_HAS_CPP23_PRINT
  std::println(stream, fmt, std::forward<Args>(args)...);
#else
  println(stream, std::format(fmt, std::forward<Args>(args)...));
#endif
}
#endif

} // namespace muduo::io
