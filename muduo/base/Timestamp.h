#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <ctime>
#include <string>
#include <utility>

namespace muduo {

class Timestamp {
public:
  using Clock = std::chrono::system_clock;
  using Microseconds = std::chrono::microseconds;
  using TimePoint = std::chrono::time_point<Clock, Microseconds>;

  constexpr Timestamp() = default;
  constexpr explicit Timestamp(std::int64_t microSecondsSinceEpochArg)
      : timePoint_(Microseconds(microSecondsSinceEpochArg)) {}
  constexpr explicit Timestamp(TimePoint tp) : timePoint_(tp) {}

  constexpr void swap(Timestamp &that) noexcept {
    std::swap(timePoint_, that.timePoint_);
  }

  [[nodiscard]] std::string toString() const;
  [[nodiscard]] std::string
  toFormattedString(bool showMicroseconds = true) const;

  [[nodiscard]] constexpr bool valid() const {
    return microSecondsSinceEpoch() > 0;
  }

  [[nodiscard]] constexpr std::int64_t microSecondsSinceEpoch() const {
    return timePoint_.time_since_epoch().count();
  }

  [[nodiscard]] constexpr std::time_t secondsSinceEpoch() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
               timePoint_.time_since_epoch())
        .count();
  }

  [[nodiscard]] constexpr TimePoint timePoint() const { return timePoint_; }

  [[nodiscard]] static Timestamp now();

  [[nodiscard]] static constexpr Timestamp invalid() { return {}; }

  [[nodiscard]] static constexpr Timestamp fromUnixTime(std::time_t t) {
    return fromUnixTime(t, 0);
  }

  [[nodiscard]] static constexpr Timestamp fromUnixTime(std::time_t t,
                                                        int microseconds) {
    return Timestamp((static_cast<std::int64_t>(t) * kMicroSecondsPerSecond) +
                     microseconds);
  }

  [[nodiscard]] auto operator<=>(const Timestamp &) const = default;

  static constexpr int kMicroSecondsPerSecond = 1000 * 1000;

private:
  TimePoint timePoint_;
};

[[nodiscard]] inline double timeDifference(Timestamp high, Timestamp low) {
  auto diff = high.timePoint() - low.timePoint();
  return std::chrono::duration<double>(diff).count();
}

[[nodiscard]] inline Timestamp addTime(Timestamp timestamp, double seconds) {
  auto delta = std::chrono::duration_cast<Timestamp::Microseconds>(
      std::chrono::duration<double>(seconds));
  return Timestamp(timestamp.timePoint() + delta);
}

} // namespace muduo
