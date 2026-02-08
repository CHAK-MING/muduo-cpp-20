#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace muduo {

class StringPiece {
public:
  constexpr StringPiece() noexcept = default;
  constexpr StringPiece(const char *str)
      : view_(str == nullptr ? std::string_view{} : std::string_view(str)) {}
  constexpr StringPiece(const char *str, size_t len)
      : view_(str == nullptr ? std::string_view{}
                             : std::string_view(str, len)) {}
  constexpr StringPiece(const std::string &str) : view_(str) {}
  constexpr StringPiece(std::string_view view) : view_(view) {}

  [[nodiscard]] constexpr const char *data() const noexcept {
    return view_.data();
  }
  [[nodiscard]] constexpr size_t size() const noexcept { return view_.size(); }
  [[nodiscard]] constexpr bool empty() const noexcept { return view_.empty(); }

  [[nodiscard]] constexpr const char *begin() const noexcept {
    return view_.begin();
  }
  [[nodiscard]] constexpr const char *end() const noexcept {
    return view_.end();
  }

  constexpr void clear() noexcept { view_ = {}; }
  constexpr void remove_prefix(size_t n) { view_.remove_prefix(n); }
  constexpr void remove_suffix(size_t n) { view_.remove_suffix(n); }

  [[nodiscard]] constexpr StringPiece
  substr(size_t pos, size_t n = std::string_view::npos) const {
    return StringPiece{view_.substr(pos, n)};
  }

  [[nodiscard]] std::string as_string() const { return std::string(view_); }
  [[nodiscard]] constexpr std::string_view as_string_view() const noexcept {
    return view_;
  }
  [[nodiscard]] constexpr int compare(StringPiece x) const noexcept {
    return view_.compare(x.view_);
  }

  [[nodiscard]] constexpr explicit operator std::string_view() const noexcept {
    return view_;
  }

private:
  std::string_view view_;
};

class StringArg {
public:
  constexpr StringArg(const char *str) : value_(str == nullptr ? "" : str) {}
  constexpr StringArg(const std::string &str) : value_(str) {}
  constexpr StringArg(StringPiece str) : value_(str.as_string_view()) {}
  constexpr StringArg(std::string_view str) : value_(str) {}

  [[nodiscard]] constexpr const char *c_str() const noexcept {
    return value_.data();
  }
  [[nodiscard]] constexpr std::string_view as_string_view() const noexcept {
    return value_;
  }

private:
  std::string_view value_;
};

} // namespace muduo
