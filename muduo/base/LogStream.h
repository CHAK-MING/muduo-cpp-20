#pragma once

#include "muduo/base/Types.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <muduo/base/noncopyable.h>
#include <span>
#include <string_view>

namespace muduo {

namespace detail {

inline constexpr size_t kSmallBuffer = 4000;
inline constexpr size_t kLargeBuffer = 4000ULL * 1000ULL;

template <size_t SIZE> class FixedBuffer : noncopyable {
public:
  FixedBuffer() = default;

  void append(const char *buf, size_t len) {
    // Append as much as possible (partial write)
    size_t n = std::min(len, static_cast<size_t>(avail()));
    std::memcpy(current(), buf, n);
    writePos_ += n;
  }

  void append(std::string_view str) { append(str.data(), str.size()); }

  [[nodiscard]] const char *data() const { return data_.data(); }
  [[nodiscard]] int length() const { return static_cast<int>(writePos_); }

  char *current() { return data_.data() + writePos_; }
  [[nodiscard]] int avail() const {
    return static_cast<int>(data_.size() - writePos_);
  }
  void add(size_t len) { writePos_ += len; }

  void reset() { writePos_ = 0; }
  void bzero() { std::ranges::fill(data_, '\0'); }

  [[nodiscard]] string toString() const {
    return string(data_.data(), static_cast<size_t>(length()));
  }
  [[nodiscard]] std::string_view toStringView() const {
    return std::string_view(data_.data(), static_cast<size_t>(length()));
  }

  [[nodiscard]] std::span<const char> readableSpan() const {
    return std::span<const char>(data_.data(), static_cast<size_t>(length()));
  }

  [[nodiscard]] std::span<char> writableSpan() {
    return std::span<char>{data_.data() + writePos_, data_.size() - writePos_};
  }

private:
  std::array<char, SIZE> data_;
  size_t writePos_ = 0;
};

} // namespace detail

class LogStream : noncopyable {
public:
  using Buffer = detail::FixedBuffer<detail::kSmallBuffer>;

  LogStream &operator<<(bool v) {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  LogStream &operator<<(char v) {
    buffer_.append(&v, 1);
    return *this;
  }

  template <std::integral T>
    requires(!std::same_as<std::remove_cvref_t<T>, bool> &&
             !std::same_as<std::remove_cvref_t<T>, char>)
  LogStream &operator<<(T v) {
    return formatInteger(v);
  }

  LogStream &operator<<(const void *p) {
    if (buffer_.avail() >= kMaxNumericSize) {
      auto out = std::format_to_n(buffer_.current(), buffer_.avail(), "{}", p);
      buffer_.add(std::min(static_cast<size_t>(out.size),
                           static_cast<size_t>(buffer_.avail())));
    }
    return *this;
  }

  template <std::floating_point T> LogStream &operator<<(T v) {
    return formatFloating(v);
  }

  LogStream &operator<<(const char *str) {
    if (str != nullptr) {
      buffer_.append(str, std::char_traits<char>::length(str));
    } else {
      buffer_.append("(null)", 6);
    }
    return *this;
  }

  LogStream &operator<<(const unsigned char *str) {
    return *this << reinterpret_cast<const char *>(str);
  }

  LogStream &operator<<(std::string_view v) {
    buffer_.append(v.data(), v.size());
    return *this;
  }

  LogStream &operator<<(const std::string &v) {
    buffer_.append(v.data(), v.size());
    return *this;
  }

  LogStream &operator<<(const Buffer &v) {
    buffer_.append(v.data(), static_cast<size_t>(v.length()));
    return *this;
  }

  void append(const char *data, int len) {
    buffer_.append(data, static_cast<size_t>(len));
  }

  void append(std::string_view v) { buffer_.append(v.data(), v.size()); }

  template <typename... Args>
  LogStream &format(std::format_string<Args...> fmt, Args &&...args) {
    if (buffer_.avail() <= 0) {
      return *this;
    }
    const auto cap = static_cast<size_t>(buffer_.avail());
    auto out =
        std::format_to_n(buffer_.current(), static_cast<std::ptrdiff_t>(cap),
                         fmt, std::forward<Args>(args)...);
    const size_t written = std::min(static_cast<size_t>(out.size), cap);
    buffer_.add(written);
    return *this;
  }

  [[nodiscard]] const Buffer &buffer() const { return buffer_; }
  void resetBuffer() { buffer_.reset(); }

private:
  static constexpr int kMaxNumericSize = 48;

  template <std::integral T> LogStream &formatInteger(T v) {
    if (buffer_.avail() >= kMaxNumericSize) {
      char *cur = buffer_.current();
      auto [ptr, ec] = std::to_chars(cur, cur + buffer_.avail(), v);
      if (ec == std::errc{}) {
        buffer_.add(static_cast<size_t>(ptr - cur));
      }
    }
    return *this;
  }

  template <std::floating_point T> LogStream &formatFloating(T v) {
    if (buffer_.avail() >= kMaxNumericSize) {
      char *cur = buffer_.current();
      char *end = cur + buffer_.avail();

      if constexpr (!std::same_as<std::remove_cvref_t<T>, long double>) {
        auto [ptr, ec] = std::to_chars(cur, end, static_cast<double>(v),
                                       std::chars_format::general, 12);
        if (ec == std::errc{}) {
          buffer_.add(static_cast<size_t>(ptr - cur));
          return *this;
        }
      }

      const auto cap = static_cast<size_t>(buffer_.avail());
      auto out =
          std::format_to_n(cur, static_cast<std::ptrdiff_t>(cap), "{:.12g}", v);
      const size_t written = std::min(static_cast<size_t>(out.size), cap);
      buffer_.add(written);
    }
    return *this;
  }

  Buffer buffer_;
};
void appendSI(LogStream &s, int64_t n);
void appendIEC(LogStream &s, int64_t n);

[[nodiscard]] string formatSI(int64_t n);
[[nodiscard]] string formatIEC(int64_t n);

} // namespace muduo
