#include "muduo/base/TimeZone.h"
#include <chrono>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace muduo;
using namespace std::string_view_literals;

struct TimeZone::Data {
  bool fixed = false;
  int eastOfUtc = 0;
  const std::chrono::time_zone *zone = nullptr;
};

namespace {

DateTime breakTime(std::int64_t t) {
  using namespace std::chrono;
  const sys_seconds tp{seconds{t}};
  const auto day_point = floor<days>(tp);
  const year_month_day ymd{day_point};
  const hh_mm_ss hms{tp - day_point};

  DateTime dt;
  dt.year = static_cast<int>(ymd.year());
  dt.month = static_cast<int>(static_cast<unsigned>(ymd.month()));
  dt.day = static_cast<int>(static_cast<unsigned>(ymd.day()));
  dt.hour = static_cast<int>(hms.hours().count());
  dt.minute = static_cast<int>(hms.minutes().count());
  dt.second = static_cast<int>(hms.seconds().count());
  return dt;
}

const std::chrono::sys_info *pickInfoNoThrow(const std::chrono::local_info &info,
                                             bool post_transition) noexcept {
  using std::chrono::local_info;
  switch (info.result) {
  case local_info::unique:
    return &info.first;
  case local_info::nonexistent:
    return post_transition ? &info.first : &info.second;
  case local_info::ambiguous:
    return post_transition ? &info.second : &info.first;
  default:
    return nullptr;
  }
}

std::string zoneFileToIanaName(std::string_view zonefile) {
  if (zonefile.empty()) {
    return {};
  }
  constexpr std::string_view kPrefix = "/usr/share/zoneinfo/";
  if (zonefile.starts_with(kPrefix) && zonefile.size() > kPrefix.size()) {
    return std::string(zonefile.substr(kPrefix.size()));
  }

  std::error_code ec;
  const auto p = std::filesystem::path(std::string(zonefile));
  if (std::filesystem::is_symlink(p, ec)) {
    const auto target = std::filesystem::read_symlink(p, ec);
    if (!ec) {
      const auto target_str = target.string();
      if (std::string_view(target_str).starts_with(kPrefix) &&
          target_str.size() > kPrefix.size()) {
        return target_str.substr(kPrefix.size());
      }
    }
  }

  return {};
}

} // namespace

std::string DateTime::toIsoString() const {
  return std::format("{:4d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", year, month,
                     day, hour, minute, second);
}

DateTime::DateTime(const struct tm &t)
    : year(t.tm_year + 1900), month(t.tm_mon + 1), day(t.tm_mday),
      hour(t.tm_hour), minute(t.tm_min), second(t.tm_sec) {}

TimeZone::TimeZone(std::unique_ptr<Data> data) : data_(std::move(data)) {}

TimeZone::TimeZone(int eastOfUtc, std::string_view /*tzname*/) {
  auto data = std::make_unique<Data>();
  data->fixed = true;
  data->eastOfUtc = eastOfUtc;
  data_ = std::move(data);
}

#if MUDUO_ENABLE_LEGACY_COMPAT
TimeZone::TimeZone(int eastOfUtc, const char *tzname)
    : TimeZone(eastOfUtc,
               std::string_view{tzname == nullptr ? "" : tzname}) {}
#endif

TimeZone::TimeZone(std::string_view zoneName) : TimeZone(loadZone(zoneName)) {}

TimeZone TimeZone::UTC() { return {0, "UTC"sv}; }

TimeZone TimeZone::China() { return {8 * 3600, "CST"sv}; }

TimeZone TimeZone::loadZone(std::string_view zoneName) {
  if (zoneName.empty()) {
    return {};
  }

  try {
    auto data = std::make_unique<Data>();
    data->zone = std::chrono::locate_zone(std::string(zoneName));
    return TimeZone(std::move(data));
  } catch (const std::runtime_error &) {
    return {};
  }
}

#if MUDUO_ENABLE_LEGACY_COMPAT
TimeZone TimeZone::loadZoneFile(const char *zonefile) {
  return zonefile == nullptr ? TimeZone{} : loadZoneFile(std::string_view{zonefile});
}
#endif

TimeZone TimeZone::loadZoneFile(std::string_view zonefile) {
  return loadZone(zoneFileToIanaName(zonefile));
}

DateTime TimeZone::toLocalTime(std::int64_t secondsSinceEpoch,
                               int *utcOffset) const {
  DateTime result;
  if (!tryToLocalTime(secondsSinceEpoch, &result, utcOffset)) {
    throw std::runtime_error("TimeZone::toLocalTime: invalid timezone");
  }
  return result;
}

std::int64_t TimeZone::fromLocalTime(const DateTime &local,
                                     bool postTransition) const {
  std::int64_t result = 0;
  if (!tryFromLocalTime(local, &result, postTransition)) {
    throw std::runtime_error("TimeZone::fromLocalTime: invalid timezone");
  }
  return result;
}

bool TimeZone::tryToLocalTime(std::int64_t secondsSinceEpoch, DateTime *result,
                              int *utcOffset) const noexcept {
  if (!valid() || result == nullptr) {
    return false;
  }

  const auto sysTp =
      std::chrono::sys_seconds{std::chrono::seconds{secondsSinceEpoch}};

  if (data_->fixed) {
    if (utcOffset != nullptr) {
      *utcOffset = data_->eastOfUtc;
    }
    *result = breakTime(secondsSinceEpoch + data_->eastOfUtc);
    return true;
  }

  const auto info = data_->zone->get_info(sysTp);
  if (utcOffset != nullptr) {
    *utcOffset = static_cast<int>(info.offset.count());
  }
  *result = breakTime(secondsSinceEpoch + info.offset.count());
  return true;
}

bool TimeZone::tryFromLocalTime(const DateTime &local, std::int64_t *result,
                                bool postTransition) const noexcept {
  if (!valid() || result == nullptr) {
    return false;
  }
  const auto localTp =
      std::chrono::local_seconds{std::chrono::seconds{fromUtcTime(local)}};
  if (data_->fixed) {
    const auto sysTp = std::chrono::sys_seconds{localTp.time_since_epoch()} -
                       std::chrono::seconds{data_->eastOfUtc};
    *result = sysTp.time_since_epoch().count();
    return true;
  }

  const auto info = data_->zone->get_info(localTp);
  const auto *picked = pickInfoNoThrow(info, postTransition);
  if (picked == nullptr) {
    return false;
  }
  const auto sysTp =
      std::chrono::sys_seconds{localTp.time_since_epoch()} - picked->offset;
  *result = sysTp.time_since_epoch().count();
  return true;
}

DateTime TimeZone::toUtcTime(std::int64_t secondsSinceEpoch) {
  return breakTime(secondsSinceEpoch);
}

std::int64_t TimeZone::fromUtcTime(const DateTime &dt) {
  using namespace std::chrono;
  const year_month_day ymd = year{dt.year} /
                             month{static_cast<unsigned>(dt.month)} /
                             day{static_cast<unsigned>(dt.day)};
  const sys_seconds tp =
      sys_days{ymd} + hours{dt.hour} + minutes{dt.minute} + seconds{dt.second};
  return tp.time_since_epoch().count();
}
