#pragma once

#include "muduo/base/Types.h"

#include <cstdint>
#include <exception>
#include <source_location>
#include <string_view>

namespace muduo {

class Exception : public std::exception {
public:
  enum class StackTraceMode : std::uint8_t { Capture, Skip };

#if MUDUO_ENABLE_LEGACY_COMPAT
  explicit Exception(const char *what);
#endif

  explicit Exception(std::string_view what);
  Exception(std::string_view what, std::source_location where) noexcept;
  Exception(std::string_view what, StackTraceMode mode,
            std::source_location where = std::source_location::current());
  ~Exception() noexcept override = default;

  [[nodiscard]] const char *what() const noexcept override {
    return message_.c_str();
  }
  [[nodiscard]] std::string_view whatView() const noexcept { return message_; }
#if MUDUO_ENABLE_LEGACY_COMPAT
  [[nodiscard]] const char *fileName() const noexcept { return fileName_.data(); }
  [[nodiscard]] const char *functionName() const noexcept {
    return functionName_.data();
  }
#else
  [[nodiscard]] std::string_view fileName() const noexcept { return fileName_; }
  [[nodiscard]] std::string_view functionName() const noexcept {
    return functionName_;
  }
#endif
  [[nodiscard]] std::uint_least32_t line() const noexcept { return line_; }
  [[nodiscard]] const char *stackTrace() const noexcept {
    return stack_.c_str();
  }
  [[nodiscard]] std::string_view stackTraceView() const noexcept {
    return stack_;
  }

private:
  string message_;
  std::string_view fileName_{"unknown"};
  std::string_view functionName_{"unknown"};
  std::uint_least32_t line_{0};
  string stack_;
};

} // namespace muduo
