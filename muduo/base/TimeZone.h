#pragma once

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <string_view>

namespace muduo {

struct DateTime {
  DateTime() = default;
  explicit DateTime(const struct tm &);
  DateTime(int _year, int _month, int _day, int _hour, int _minute, int _second)
      : year(_year), month(_month), day(_day), hour(_hour), minute(_minute),
        second(_second) {}

  [[nodiscard]] std::string toIsoString() const;

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
};

class TimeZone {
public:
  TimeZone() = default;
  TimeZone(int eastOfUtc, const char *tzname);
  explicit TimeZone(std::string_view zoneName);

  static TimeZone UTC();
  static TimeZone China();
  static TimeZone loadZone(std::string_view zoneName);
  static TimeZone loadZoneFile(const char *zonefile);
  static TimeZone loadZoneFile(std::string_view zonefile);

  [[nodiscard]] bool valid() const { return static_cast<bool>(data_); }

  [[nodiscard]] DateTime toLocalTime(std::int64_t secondsSinceEpoch,
                                     int *utcOffset = nullptr) const;
  [[nodiscard]] std::int64_t fromLocalTime(const DateTime &local,
                                           bool postTransition = false) const;

  static DateTime toUtcTime(std::int64_t secondsSinceEpoch);
  static std::int64_t fromUtcTime(const DateTime &dt);

  struct Data;

private:
  explicit TimeZone(std::unique_ptr<Data> data);

  std::shared_ptr<Data> data_;

  friend class TimeZoneTestPeer;
};

} // namespace muduo
