#pragma once

#include "muduo/base/Types.h"

#include <chrono>
#include <ctime>
#include <utility>

namespace muduo {

class Date {
public:
  struct YearMonthDay {
    int year;
    int month;
    int day;
  };

  static constexpr int kDaysPerWeek = 7;
  static constexpr int kJulianDayOf1970_01_01 = 2440588;
  static constexpr std::chrono::sys_days kInvalidDate{
      std::chrono::days{-kJulianDayOf1970_01_01}};

  constexpr Date() : day_(kInvalidDate) {}
  constexpr Date(int year, int month, int day)
      : Date(std::chrono::year{year} /
             std::chrono::month{static_cast<unsigned>(month)} /
             std::chrono::day{static_cast<unsigned>(day)}) {}
  constexpr explicit Date(std::chrono::year_month_day ymd)
      : day_(ymd.ok() ? std::chrono::sys_days{ymd} : kInvalidDate) {}
  constexpr explicit Date(int julianDayNum)
      : day_(julianDayNum > 0 ? fromJulianDay(julianDayNum) : kInvalidDate) {}
  explicit Date(const struct tm &);

  constexpr void swap(Date &that) noexcept {
    std::swap(day_, that.day_);
  }

  [[nodiscard]] constexpr bool valid() const { return day_ != kInvalidDate; }
  [[nodiscard]] string toIsoString() const;
  [[nodiscard]] constexpr YearMonthDay yearMonthDay() const {
    if (!valid()) {
      return YearMonthDay{.year = 0, .month = 0, .day = 0};
    }
    const std::chrono::year_month_day ymd{day_};
    return YearMonthDay{.year = static_cast<int>(ymd.year()),
                        .month = static_cast<int>(static_cast<unsigned>(ymd.month())),
                        .day = static_cast<int>(static_cast<unsigned>(ymd.day()))};
  }
  [[nodiscard]] constexpr std::chrono::year_month_day asChronoDate() const {
    if (!valid()) {
      return {};
    }
    return std::chrono::year_month_day{day_};
  }

  [[nodiscard]] constexpr int year() const { return yearMonthDay().year; }
  [[nodiscard]] constexpr int month() const { return yearMonthDay().month; }
  [[nodiscard]] constexpr int day() const { return yearMonthDay().day; }

  [[nodiscard]] constexpr int weekDay() const {
    return (julianDayNumber() + 1) % kDaysPerWeek;
  }

  [[nodiscard]] constexpr int julianDayNumber() const {
    return toJulianDay(day_);
  }

  [[nodiscard]] constexpr auto operator<=>(const Date &rhs) const {
    return day_ <=> rhs.day_;
  }
  [[nodiscard]] constexpr bool operator==(const Date &rhs) const {
    return day_ == rhs.day_;
  }

private:
  static constexpr std::chrono::sys_days fromJulianDay(int julianDayNum) {
    return std::chrono::sys_days{
        std::chrono::days{julianDayNum - kJulianDayOf1970_01_01}};
  }
  static constexpr int toJulianDay(std::chrono::sys_days day) {
    return static_cast<int>(day.time_since_epoch().count()) +
           kJulianDayOf1970_01_01;
  }

  std::chrono::sys_days day_;
};

} // namespace muduo
