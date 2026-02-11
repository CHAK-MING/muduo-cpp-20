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

  explicit Exception(string what);
  explicit Exception(const char *what);
  explicit Exception(std::string_view what);
  Exception(std::string_view what, std::source_location where) noexcept;
  Exception(std::string_view what, StackTraceMode mode,
            std::source_location where = std::source_location::current());
  ~Exception() noexcept override = default;

  [[nodiscard]] const char *what() const noexcept override {
    return message_.c_str();
  }
  [[nodiscard]] std::string_view whatView() const noexcept { return message_; }
  [[nodiscard]] const char *fileName() const noexcept { return fileName_; }
  [[nodiscard]] const char *functionName() const noexcept {
    return functionName_;
  }
  [[nodiscard]] std::uint_least32_t line() const noexcept { return line_; }
  [[nodiscard]] const char *stackTrace() const noexcept {
    return stack_.c_str();
  }
  [[nodiscard]] std::string_view stackTraceView() const noexcept {
    return stack_;
  }

private:
  string message_;
  const char *fileName_{"unknown"};
  const char *functionName_{"unknown"};
  std::uint_least32_t line_{0};
  string stack_;
};

} // namespace muduo
