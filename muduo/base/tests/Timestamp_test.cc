#include "muduo/base/Timestamp.h"

#include <gtest/gtest.h>

#include <ctime>

using muduo::Timestamp;

TEST(Timestamp, BasicProperties) {
  const auto now = Timestamp::now();
  EXPECT_TRUE(now.valid());
  EXPECT_GT(now.microSecondsSinceEpoch(), 0);
  EXPECT_GT(now.secondsSinceEpoch(), 0);
}

TEST(Timestamp, FromUnixTime) {
  constexpr std::time_t kSec = 1'700'000'000;
  constexpr int kMicros = 123456;
  const auto ts = Timestamp::fromUnixTime(kSec, kMicros);
  EXPECT_EQ(ts.secondsSinceEpoch(), kSec);
  EXPECT_EQ(ts.microSecondsSinceEpoch(),
            static_cast<std::int64_t>(kSec) * Timestamp::kMicroSecondsPerSecond +
                kMicros);
}

TEST(Timestamp, AddAndDifference) {
  const auto t0 = Timestamp::fromUnixTime(100, 0);
  const auto t1 = muduo::addTime(t0, 2.5);
  EXPECT_DOUBLE_EQ(muduo::timeDifference(t1, t0), 2.5);
  EXPECT_TRUE(t1 > t0);
}

TEST(Timestamp, StringFormatsAndSwap) {
  const auto t = Timestamp::fromUnixTime(1'700'000'000, 123456);
  EXPECT_EQ(t.toString(), "1700000000.123456");
  EXPECT_EQ(t.toFormattedString(false).size(), 17U);
  EXPECT_EQ(t.toFormattedString(true).size(), 24U);

  auto a = Timestamp::fromUnixTime(1, 1);
  auto b = Timestamp::fromUnixTime(2, 2);
  a.swap(b);
  EXPECT_EQ(a.secondsSinceEpoch(), 2);
  EXPECT_EQ(b.secondsSinceEpoch(), 1);
}

TEST(Timestamp, InvalidTimestamp) {
  const auto inv = Timestamp::invalid();
  EXPECT_FALSE(inv.valid());
  EXPECT_EQ(inv.microSecondsSinceEpoch(), 0);
}

TEST(Timestamp, MonotonicNowOneMillionSamples) {
  constexpr int kSamples = 1'000'000;
  auto last = Timestamp::now();
  for (int i = 0; i < kSamples; ++i) {
    const auto now = Timestamp::now();
    EXPECT_GE(now, last);
    last = now;
  }
}
