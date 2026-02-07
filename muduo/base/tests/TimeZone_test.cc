#include "muduo/base/TimeZone.h"

#include <gtest/gtest.h>

#include <ctime>
#include <filesystem>

namespace {

std::int64_t toEpoch(const muduo::DateTime& dt) { return muduo::TimeZone::fromUtcTime(dt); }

std::int64_t parseGmt(const char* s) {
  std::tm tm{};
  EXPECT_NE(::strptime(s, "%Y-%m-%d %H:%M:%S", &tm), nullptr);
  return static_cast<std::int64_t>(::timegm(&tm));
}

} // namespace

TEST(TimeZone, FixedOffsetRoundTrip) {
  muduo::TimeZone tz(8 * 3600, "CST");
  const std::int64_t gmt = toEpoch(muduo::DateTime(2014, 4, 3, 0, 0, 0));

  int offset = 0;
  const auto local = tz.toLocalTime(gmt, &offset);
  EXPECT_EQ(offset, 8 * 3600);
  EXPECT_EQ(local.toIsoString(), "2014-04-03 08:00:00");

  EXPECT_EQ(tz.fromLocalTime(local), gmt);
}

TEST(TimeZone, UtcStaticRoundTrip) {
  const muduo::DateTime dt(2024, 2, 29, 12, 34, 56);
  const auto epoch = muduo::TimeZone::fromUtcTime(dt);
  const auto out = muduo::TimeZone::toUtcTime(epoch);
  EXPECT_EQ(out.toIsoString(), dt.toIsoString());
}

TEST(TimeZone, LoadZoneAndZoneFile) {
  const auto shanghai = muduo::TimeZone::loadZone("Asia/Shanghai");
  ASSERT_TRUE(shanghai.valid());

  const std::int64_t gmt = toEpoch(muduo::DateTime(2020, 1, 1, 0, 0, 0));
  int offset = 0;
  const auto local = shanghai.toLocalTime(gmt, &offset);
  EXPECT_EQ(offset, 8 * 3600);
  EXPECT_EQ(local.toIsoString(), "2020-01-01 08:00:00");

  constexpr const char* kUtcZoneFile = "/usr/share/zoneinfo/UTC";
  if (!std::filesystem::exists(kUtcZoneFile)) {
    GTEST_SKIP() << "zoneinfo file not found: " << kUtcZoneFile;
  }
  const auto utc = muduo::TimeZone::loadZoneFile(kUtcZoneFile);
  ASSERT_TRUE(utc.valid());
  EXPECT_EQ(utc.toLocalTime(gmt).toIsoString(), muduo::TimeZone::toUtcTime(gmt).toIsoString());
}

TEST(TimeZone, LosAngelesDstFromLocalTime) {
  const auto la = muduo::TimeZone::loadZone("America/Los_Angeles");
  if (!la.valid()) {
    GTEST_SKIP() << "zone not available: America/Los_Angeles";
  }

  struct Case {
    const char* gmt;
    muduo::DateTime local;
    bool postTransition;
  };

  const Case cases[] = {
      {"2022-03-13 09:59:59", muduo::DateTime(2022, 3, 13, 1, 59, 59), false},
      {"2022-03-13 10:00:00", muduo::DateTime(2022, 3, 13, 3, 0, 0), false},
      {"2022-11-06 08:00:00", muduo::DateTime(2022, 11, 6, 1, 0, 0), false},
      {"2022-11-06 09:00:00", muduo::DateTime(2022, 11, 6, 1, 0, 0), true},
  };

  for (const auto& c : cases) {
    EXPECT_EQ(la.fromLocalTime(c.local, c.postTransition), parseGmt(c.gmt));
  }
}

TEST(TimeZone, InvalidZoneHandling) {
  const auto invalid = muduo::TimeZone::loadZone("Not/A_Real_Zone");
  EXPECT_FALSE(invalid.valid());
  EXPECT_FALSE(muduo::TimeZone::loadZone("").valid());
  EXPECT_FALSE(muduo::TimeZone::loadZoneFile("/definitely/not/exist").valid());

  EXPECT_THROW((void)invalid.toLocalTime(0), std::runtime_error);
  EXPECT_THROW((void)invalid.fromLocalTime(muduo::DateTime(2024, 1, 1, 0, 0, 0)),
               std::runtime_error);
}
