#include "muduo/base/Date.h"

#include <format>

using namespace muduo;

Date::Date(const struct tm &t)
    : Date(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) {}

string Date::toIsoString() const {
  if (!valid()) {
    return "0000-00-00";
  }
  return std::format("{:%F}", std::chrono::sys_days{asChronoDate()});
}
