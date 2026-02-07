#include "muduo/base/Date.h"

#include <gtest/gtest.h>

#include <array>
#include <ctime>

namespace {

constexpr bool isLeapYear(int year) {
  if (year % 400 == 0) {
    return true;
  }
  if (year % 100 == 0) {
    return false;
  }
  return year % 4 == 0;
}

constexpr int daysOfMonth(int year, int month) {
  constexpr std::array<std::array<int, 13>, 2> kDays = {{
      {{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}},
      {{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}},
  }};
  return kDays[isLeapYear(year) ? 1 : 0][month];
}

} // namespace

TEST(Date, BasicRoundTripAndWeekday) {
  int julianDay = 2415021;
  int weekday = 1; // Monday

  for (int year = 1900; year < 2050; ++year) {
    EXPECT_EQ(muduo::Date(year, 3, 1).julianDayNumber() -
                  muduo::Date(year, 2, 28).julianDayNumber(),
              isLeapYear(year) ? 2 : 1);

    for (int month = 1; month <= 12; ++month) {
      for (int day = 1; day <= daysOfMonth(year, month); ++day) {
        const muduo::Date d(year, month, day);
        EXPECT_EQ(d.year(), year);
        EXPECT_EQ(d.month(), month);
        EXPECT_EQ(d.day(), day);
        EXPECT_EQ(d.weekDay(), weekday);
        EXPECT_EQ(d.julianDayNumber(), julianDay);

        const muduo::Date d2(julianDay);
        EXPECT_EQ(d2.year(), year);
        EXPECT_EQ(d2.month(), month);
        EXPECT_EQ(d2.day(), day);
        EXPECT_EQ(d2.weekDay(), weekday);

        ++julianDay;
        weekday = (weekday + 1) % 7;
      }
    }
  }
}

TEST(Date, IsoString) {
  const muduo::Date d(2024, 2, 29);
  EXPECT_EQ(d.toIsoString(), "2024-02-29");
}

TEST(Date, InvalidAndComparisonAndSwap) {
  muduo::Date invalid;
  EXPECT_FALSE(invalid.valid());
  EXPECT_EQ(invalid.toIsoString(), "0000-00-00");

  muduo::Date a(2020, 1, 2);
  muduo::Date b(2021, 1, 2);
  EXPECT_LT(a, b);
  a.swap(b);
  EXPECT_EQ(a.year(), 2021);
  EXPECT_EQ(b.year(), 2020);
}

TEST(Date, ConstructFromTm) {
  std::tm tm{};
  tm.tm_year = 124;
  tm.tm_mon = 1;
  tm.tm_mday = 29;

  const muduo::Date d(tm);
  EXPECT_TRUE(d.valid());
  EXPECT_EQ(d.toIsoString(), "2024-02-29");
}
