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
  [[deprecated("Use TimerId(sequence) to avoid raw pointer coupling.")]]
  constexpr TimerId([[maybe_unused]] Timer *timer, std::int64_t seq) noexcept
      : sequence_(seq) {}

  [[nodiscard]] constexpr bool valid() const noexcept { return sequence_ > 0; }
  [[nodiscard]] constexpr std::int64_t sequence() const noexcept {
    return sequence_;
  }
  [[nodiscard]] constexpr auto operator<=>(const TimerId &) const noexcept =
      default;
  [[nodiscard]] constexpr bool operator==(const TimerId &) const noexcept =
      default;

private:
  std::int64_t sequence_{0};
};

} // namespace muduo::net
