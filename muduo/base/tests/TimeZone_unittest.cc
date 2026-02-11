#include "muduo/base/TimeZone.h"

#include <gtest/gtest.h>

#include <ctime>
#include <filesystem>
#include <string_view>

using namespace std::string_view_literals;

namespace {

std::int64_t toEpoch(const muduo::DateTime& dt) { return muduo::TimeZone::fromUtcTime(dt); }

std::int64_t parseGmt(const char* s) {
  std::tm tm{};
  EXPECT_NE(::strptime(s, "%Y-%m-%d %H:%M:%S", &tm), nullptr);
  return static_cast<std::int64_t>(::timegm(&tm));
}

muduo::DateTime parseLocal(const char* s) {
  std::tm tm{};
  EXPECT_NE(::strptime(s, "%Y-%m-%d %H:%M:%S", &tm), nullptr);
  return muduo::DateTime(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                         tm.tm_min, tm.tm_sec);
}

} // namespace

TEST(TimeZoneCompatibility, FixedOffsetRoundTrip) {
  muduo::TimeZone tz(8 * 3600, "CST"sv);
  const std::int64_t gmt = toEpoch(muduo::DateTime(2014, 4, 3, 0, 0, 0));

  int offset = 0;
  const auto local = tz.toLocalTime(gmt, &offset);
  EXPECT_EQ(offset, 8 * 3600);
  EXPECT_EQ(local.toIsoString(), "2014-04-03 08:00:00");

  EXPECT_EQ(tz.fromLocalTime(local), gmt);
}

TEST(TimeZoneCompatibility, UtcStaticRoundTrip) {
  const muduo::DateTime dt(2024, 2, 29, 12, 34, 56);
  const auto epoch = muduo::TimeZone::fromUtcTime(dt);
  const auto out = muduo::TimeZone::toUtcTime(epoch);
  EXPECT_EQ(out.toIsoString(), dt.toIsoString());
}

TEST(TimeZoneCompatibility, LoadZoneAndZoneFile) {
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
  const auto utc = muduo::TimeZone::loadZoneFile(std::string_view{kUtcZoneFile});
  ASSERT_TRUE(utc.valid());
  EXPECT_EQ(utc.toLocalTime(gmt).toIsoString(), muduo::TimeZone::toUtcTime(gmt).toIsoString());
}

TEST(TimeZoneCompatibility, LosAngelesDstFromLocalTime) {
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

TEST(TimeZoneCompatibility, EuropeLondonDstRoundTrip) {
  const auto london = muduo::TimeZone::loadZone("Europe/London");
  if (!london.valid()) {
    GTEST_SKIP() << "zone not available: Europe/London";
  }

  struct Case {
    const char* gmt;
    const char* local;
    int offset;
    bool postTransition;
  };

  const Case cases[] = {
      {"2011-03-27 00:59:59", "2011-03-27 00:59:59", 0, false},
      {"2011-03-27 01:00:00", "2011-03-27 02:00:00", 3600, false},
      {"2011-10-30 00:59:59", "2011-10-30 01:59:59", 3600, false},
      {"2011-10-30 01:00:00", "2011-10-30 01:00:00", 0, true},
  };

  for (const auto& c : cases) {
    const auto gmt = parseGmt(c.gmt);
    int offset = 0;
    const auto local = london.toLocalTime(gmt, &offset);
    EXPECT_EQ(local.toIsoString(), c.local);
    EXPECT_EQ(offset, c.offset);
    EXPECT_EQ(london.fromLocalTime(parseLocal(c.local), c.postTransition), gmt);
  }
}

TEST(TimeZoneCompatibility, AmericaNewYorkDstRoundTrip) {
  const auto ny = muduo::TimeZone::loadZone("America/New_York");
  if (!ny.valid()) {
    GTEST_SKIP() << "zone not available: America/New_York";
  }

  struct Case {
    const char* gmt;
    const char* local;
    int offset;
    bool postTransition;
  };

  const Case cases[] = {
      {"2007-03-11 06:59:59", "2007-03-11 01:59:59", -5 * 3600, false},
      {"2007-03-11 07:00:00", "2007-03-11 03:00:00", -4 * 3600, false},
      {"2007-11-04 05:59:59", "2007-11-04 01:59:59", -4 * 3600, false},
      {"2007-11-04 06:00:00", "2007-11-04 01:00:00", -5 * 3600, true},
  };

  for (const auto& c : cases) {
    const auto gmt = parseGmt(c.gmt);
    int offset = 0;
    const auto local = ny.toLocalTime(gmt, &offset);
    EXPECT_EQ(local.toIsoString(), c.local);
    EXPECT_EQ(offset, c.offset);
    EXPECT_EQ(ny.fromLocalTime(parseLocal(c.local), c.postTransition), gmt);
  }
}

TEST(TimeZoneCompatibility, AsiaHongKongRoundTrip) {
  const auto hk = muduo::TimeZone::loadZone("Asia/Hong_Kong");
  if (!hk.valid()) {
    GTEST_SKIP() << "zone not available: Asia/Hong_Kong";
  }

  struct Case {
    const char* gmt;
    const char* local;
    int offset;
  };

  const Case cases[] = {
      {"2011-04-03 00:00:00", "2011-04-03 08:00:00", 8 * 3600},
      {"2020-01-01 00:00:00", "2020-01-01 08:00:00", 8 * 3600},
  };

  for (const auto& c : cases) {
    const auto gmt = parseGmt(c.gmt);
    int offset = 0;
    const auto local = hk.toLocalTime(gmt, &offset);
    EXPECT_EQ(local.toIsoString(), c.local);
    EXPECT_EQ(offset, c.offset);
    EXPECT_EQ(hk.fromLocalTime(parseLocal(c.local)), gmt);
  }
}

TEST(TimeZoneCompatibility, AustraliaSydneyDstRoundTrip) {
  const auto sydney = muduo::TimeZone::loadZone("Australia/Sydney");
  if (!sydney.valid()) {
    GTEST_SKIP() << "zone not available: Australia/Sydney";
  }

  struct Case {
    const char* gmt;
    const char* local;
    int offset;
    bool postTransition;
  };

  const Case cases[] = {
      {"2011-04-02 15:59:59", "2011-04-03 02:59:59", 11 * 3600, false},
      {"2011-04-02 16:00:00", "2011-04-03 02:00:00", 10 * 3600, true},
      {"2011-10-01 15:59:59", "2011-10-02 01:59:59", 10 * 3600, false},
      {"2011-10-01 16:00:00", "2011-10-02 03:00:00", 11 * 3600, false},
  };

  for (const auto& c : cases) {
    const auto gmt = parseGmt(c.gmt);
    int offset = 0;
    const auto local = sydney.toLocalTime(gmt, &offset);
    EXPECT_EQ(local.toIsoString(), c.local);
    EXPECT_EQ(offset, c.offset);
    EXPECT_EQ(sydney.fromLocalTime(parseLocal(c.local), c.postTransition), gmt);
  }
}

TEST(TimeZoneCompatibility, InvalidZoneHandling) {
  const auto invalid = muduo::TimeZone::loadZone("Not/A_Real_Zone");
  EXPECT_FALSE(invalid.valid());
  EXPECT_FALSE(muduo::TimeZone::loadZone("").valid());
  EXPECT_FALSE(muduo::TimeZone::loadZoneFile("/definitely/not/exist"sv).valid());

  EXPECT_THROW((void)invalid.toLocalTime(0), std::runtime_error);
  EXPECT_THROW((void)invalid.fromLocalTime(muduo::DateTime(2024, 1, 1, 0, 0, 0)),
               std::runtime_error);
}
