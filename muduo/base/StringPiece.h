#if MUDUO_ENABLE_LEGACY_COMPAT
#pragma once

#include <compare>
#include <cstddef>
#include <string>
#include <string_view>

namespace muduo {

class StringPiece {
public:
  using size_type = std::string_view::size_type;

  constexpr StringPiece() noexcept = default;
  explicit constexpr StringPiece(const char *str)
      : view_(str == nullptr ? std::string_view{} : std::string_view(str)) {}
  explicit constexpr StringPiece(const char *str, size_t len)
      : view_(str == nullptr ? std::string_view{}
                             : std::string_view(str, len)) {}
  explicit constexpr StringPiece(const std::string &str) : view_(str) {}
  explicit constexpr StringPiece(std::string_view view) : view_(view) {}

  [[nodiscard]] constexpr const char *data() const noexcept {
    return view_.data();
  }
  [[nodiscard]] constexpr size_t size() const noexcept { return view_.size(); }
  [[nodiscard]] constexpr int sizeInt() const noexcept {
    return static_cast<int>(view_.size());
  }
  [[nodiscard]] constexpr bool empty() const noexcept { return view_.empty(); }

  [[nodiscard]] constexpr const char *begin() const noexcept {
    return view_.begin();
  }
  [[nodiscard]] constexpr const char *end() const noexcept {
    return view_.end();
  }

  constexpr void clear() noexcept { view_ = {}; }
  void set(const char *str) noexcept {
    view_ = (str == nullptr) ? std::string_view{} : std::string_view(str);
  }
  constexpr void set(const char *str, size_t len) noexcept {
    view_ = (str == nullptr) ? std::string_view{} : std::string_view(str, len);
  }
  void set(const std::string &str) noexcept { view_ = str; }
  constexpr void set(const unsigned char *str, size_t len) noexcept {
    view_ = (str == nullptr)
                ? std::string_view{}
                : std::string_view(reinterpret_cast<const char *>(str), len);
  }
  constexpr void remove_prefix(size_t n) { view_.remove_prefix(n); }
  constexpr void remove_suffix(size_t n) { view_.remove_suffix(n); }
  [[nodiscard]] constexpr char operator[](size_t i) const noexcept {
    return view_[i];
  }
  [[nodiscard]] constexpr StringPiece
  substr(size_t pos, size_t n = std::string_view::npos) const {
    return StringPiece{view_.substr(pos, n)};
  }
  [[nodiscard]] std::string as_string() const { return std::string(view_); }
  void CopyToString(std::string *target) const {
    if (target != nullptr) {
      target->assign(view_);
    }
  }
  [[nodiscard]] constexpr std::string_view as_string_view() const noexcept {
    return view_;
  }
  [[nodiscard]] constexpr bool starts_with(StringPiece x) const noexcept {
    return view_.starts_with(x.view_);
  }
  [[nodiscard]] constexpr bool starts_with(char c) const noexcept {
    return view_.starts_with(c);
  }
  [[nodiscard]] bool starts_with(const char *s) const noexcept {
    return view_.starts_with(s == nullptr ? std::string_view{}
                                          : std::string_view(s));
  }
  [[nodiscard]] constexpr int compare(StringPiece x) const noexcept {
    return view_.compare(x.view_);
  }

  [[nodiscard]] constexpr explicit operator std::string_view() const noexcept {
    return view_;
  }

  [[nodiscard]] constexpr auto
  operator<=>(const StringPiece &) const noexcept = default;
  [[nodiscard]] constexpr bool
  operator==(const StringPiece &) const noexcept = default;

private:
  std::string_view view_;
};

class StringArg {
public:
  explicit constexpr StringArg(const char *str)
      : value_(str == nullptr ? "" : str) {}
  explicit constexpr StringArg(const std::string &str) : value_(str) {}
  explicit constexpr StringArg(StringPiece str)
      : value_(str.as_string_view()) {}
  explicit constexpr StringArg(std::string_view str) : value_(str) {}

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

#endif