#pragma once

#include "muduo/base/Types.h"

#include <exception>
#include <string_view>

namespace muduo {

class Exception : public std::exception {
public:
  explicit Exception(std::string_view what);
  ~Exception() noexcept override = default;

  [[nodiscard]] const char *what() const noexcept override {
    return message_.c_str();
  }
  [[nodiscard]] std::string_view whatView() const noexcept { return message_; }
  [[nodiscard]] const char *stackTrace() const noexcept {
    return stack_.c_str();
  }
  [[nodiscard]] std::string_view stackTraceView() const noexcept {
    return stack_;
  }

private:
  string message_;
  string stack_;
};

} // namespace muduo
