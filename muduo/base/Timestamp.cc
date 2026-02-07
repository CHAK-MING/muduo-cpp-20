#include "muduo/base/Timestamp.h"

#include <chrono>
#include <format>

using namespace muduo;

std::string Timestamp::toString() const {
  const int64_t micros = microSecondsSinceEpoch();
  const int64_t seconds = micros / kMicroSecondsPerSecond;
  const int64_t microseconds = micros % kMicroSecondsPerSecond;
  return std::format("{}.{:06}", seconds, microseconds);
}

std::string Timestamp::toFormattedString(bool showMicroseconds) const {
  const auto tp = timePoint();
  const auto secTp = std::chrono::floor<std::chrono::seconds>(tp);
  const auto us = tp - secTp;

  if (showMicroseconds) {
    return std::format("{:%Y%m%d %H:%M:%S}.{:06d}", secTp, us.count());
  }
  return std::format("{:%Y%m%d %H:%M:%S}", secTp);
}

Timestamp Timestamp::now() {
  return Timestamp(
      std::chrono::time_point_cast<std::chrono::microseconds>(Clock::now()));
}
