#pragma once

#include "muduo/base/copyable.h"

#include <compare>
#include <cstdint>

namespace muduo::net {

class Timer;

class TimerId : public muduo::copyable {
public:
  constexpr TimerId() noexcept = default;
  explicit constexpr TimerId(std::int64_t seq) noexcept : sequence_(seq) {}
#if MUDUO_ENABLE_LEGACY_COMPAT
  constexpr TimerId([[maybe_unused]] Timer *timer, std::int64_t seq) noexcept
      : sequence_(seq) {}
#endif

  [[nodiscard]] constexpr bool valid() const noexcept { return sequence_ > 0; }
  [[nodiscard]] constexpr std::int64_t sequence() const noexcept {
    return sequence_;
  }
  [[nodiscard]] constexpr std::strong_ordering
  operator<=>(const TimerId &rhs) const noexcept {
    return sequence_ <=> rhs.sequence_;
  }
  [[nodiscard]] constexpr bool operator==(const TimerId &rhs) const noexcept {
    return sequence_ == rhs.sequence_;
  }

private:
  std::int64_t sequence_{0};
};

} // namespace muduo::net
