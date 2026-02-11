#include "muduo/base/LogStream.h"

#include <algorithm>
#include <format>
#include <iterator>
#include <utility>

using namespace muduo;

namespace {

template <typename Append> void appendSIImpl(Append &append, int64_t n) {
  const auto d = static_cast<double>(n);
  if (n < 1000) {
    append("{}", n);
  } else if (n < 9995) {
    append("{:.2f}k", d / 1e3);
  } else if (n < 99950) {
    append("{:.1f}k", d / 1e3);
  } else if (n < 999500) {
    append("{:.0f}k", d / 1e3);
  } else if (n < 9995000) {
    append("{:.2f}M", d / 1e6);
  } else if (n < 99950000) {
    append("{:.1f}M", d / 1e6);
  } else if (n < 999500000) {
    append("{:.0f}M", d / 1e6);
  } else if (n < 9995000000) {
    append("{:.2f}G", d / 1e9);
  } else if (n < 99950000000) {
    append("{:.1f}G", d / 1e9);
  } else if (n < 999500000000) {
    append("{:.0f}G", d / 1e9);
  } else if (n < 9995000000000) {
    append("{:.2f}T", d / 1e12);
  } else if (n < 99950000000000) {
    append("{:.1f}T", d / 1e12);
  } else if (n < 999500000000000) {
    append("{:.0f}T", d / 1e12);
  } else if (n < 9995000000000000) {
    append("{:.2f}P", d / 1e15);
  } else if (n < 99950000000000000) {
    append("{:.1f}P", d / 1e15);
  } else if (n < 999500000000000000) {
    append("{:.0f}P", d / 1e15);
  } else {
    append("{:.2f}E", d / 1e18);
  }
}

template <typename Append> void appendIECImpl(Append &append, int64_t n) {
  const auto d = static_cast<double>(n);
  constexpr double Ki = 1024.0;
  constexpr double Mi = Ki * 1024.0;
  constexpr double Gi = Mi * 1024.0;
  constexpr double Ti = Gi * 1024.0;
  constexpr double Pi = Ti * 1024.0;
  constexpr double Ei = Pi * 1024.0;

  if (d < Ki) {
    append("{}", n);
  } else if (d < Ki * 9.995) {
    append("{:.2f}Ki", d / Ki);
  } else if (d < Ki * 99.95) {
    append("{:.1f}Ki", d / Ki);
  } else if (d < Ki * 1023.5) {
    append("{:.0f}Ki", d / Ki);
  } else if (d < Mi * 9.995) {
    append("{:.2f}Mi", d / Mi);
  } else if (d < Mi * 99.95) {
    append("{:.1f}Mi", d / Mi);
  } else if (d < Mi * 1023.5) {
    append("{:.0f}Mi", d / Mi);
  } else if (d < Gi * 9.995) {
    append("{:.2f}Gi", d / Gi);
  } else if (d < Gi * 99.95) {
    append("{:.1f}Gi", d / Gi);
  } else if (d < Gi * 1023.5) {
    append("{:.0f}Gi", d / Gi);
  } else if (d < Ti * 9.995) {
    append("{:.2f}Ti", d / Ti);
  } else if (d < Ti * 99.95) {
    append("{:.1f}Ti", d / Ti);
  } else if (d < Ti * 1023.5) {
    append("{:.0f}Ti", d / Ti);
  } else if (d < Pi * 9.995) {
    append("{:.2f}Pi", d / Pi);
  } else if (d < Pi * 99.95) {
    append("{:.1f}Pi", d / Pi);
  } else if (d < Pi * 1023.5) {
    append("{:.0f}Pi", d / Pi);
  } else if (d < Ei * 9.995) {
    append("{:.2f}Ei", d / Ei);
  } else {
    append("{:.1f}Ei", d / Ei);
  }
}

} // namespace

void muduo::appendSI(LogStream &s, int64_t n) {
  auto append = [&s]<typename... Args>(std::format_string<Args...> fmt,
                                       Args &&...args) {
    s.format(fmt, std::forward<Args>(args)...);
  };
  appendSIImpl(append, n);
}

void muduo::appendIEC(LogStream &s, int64_t n) {
  auto append = [&s]<typename... Args>(std::format_string<Args...> fmt,
                                       Args &&...args) {
    s.format(fmt, std::forward<Args>(args)...);
  };
  appendIECImpl(append, n);
}

string muduo::formatSI(int64_t n) {
  string out;
  out.reserve(32);
  auto append = [&out]<typename... Args>(std::format_string<Args...> fmt,
                                         Args &&...args) {
    std::format_to(std::back_inserter(out), fmt, std::forward<Args>(args)...);
  };
  appendSIImpl(append, n);
  return out;
}

string muduo::formatIEC(int64_t n) {
  string out;
  out.reserve(32);
  auto append = [&out]<typename... Args>(std::format_string<Args...> fmt,
                                         Args &&...args) {
    std::format_to(std::back_inserter(out), fmt, std::forward<Args>(args)...);
  };
  appendIECImpl(append, n);
  return out;
}
