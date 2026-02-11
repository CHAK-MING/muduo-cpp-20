#include "muduo/net/inspect/ProcessInspector.h"

#include "muduo/base/FileUtil.h"
#include "muduo/base/ProcessInfo.h"

#include <charconv>
#include <climits>
#include <format>

namespace muduo::net::inspect {

string uptime(Timestamp now, Timestamp start, bool showMicroseconds) {
  const int64_t age =
      now.microSecondsSinceEpoch() - start.microSecondsSinceEpoch();
  const int seconds = static_cast<int>(age / Timestamp::kMicroSecondsPerSecond);
  const int days = seconds / 86400;
  const int hours = (seconds % 86400) / 3600;
  const int minutes = (seconds % 3600) / 60;
  if (showMicroseconds) {
    const int microseconds = static_cast<int>(age % Timestamp::kMicroSecondsPerSecond);
    return std::format("{} days {:02}:{:02}:{:02}.{:06}", days, hours, minutes,
                       seconds % 60, microseconds);
  } else {
    return std::format("{} days {:02}:{:02}:{:02}", days, hours, minutes,
                       seconds % 60);
  }
}

long getLong(const string &content, std::string_view key) {
  const size_t pos = content.find(key);
  if (pos == string::npos) {
    return 0;
  }
  const char *begin = content.data() + static_cast<std::ptrdiff_t>(pos + key.size());
  while (*begin == ' ' || *begin == '\t') {
    ++begin;
  }

  long result = 0;
  const char *end = content.data() + static_cast<std::ptrdiff_t>(content.size());
  const auto [ptr, ec] = std::from_chars(begin, end, result);
  if (ec != std::errc{} || ptr == begin) {
    return 0;
  }
  return result;
}

std::string_view nextField(std::string_view data) {
  const size_t pos = data.find(' ');
  if (pos == std::string_view::npos || pos + 1 >= data.size()) {
    return {};
  }
  data.remove_prefix(pos + 1);
  return data;
}

ProcessInfo::CpuTime getCpuTime(std::string_view data) {
  ProcessInfo::CpuTime t;
  for (int i = 0; i < 10; ++i) {
    data = nextField(data);
    if (data.empty()) {
      return t;
    }
  }

  long utime = 0;
  auto [uptr, uec] = std::from_chars(data.data(), data.data() + data.size(), utime);
  if (uec != std::errc{}) {
    return t;
  }
  data = nextField(data);
  if (data.empty()) {
    return t;
  }

  long stime = 0;
  auto [sptr, sec] = std::from_chars(data.data(), data.data() + data.size(), stime);
  if (sec != std::errc{}) {
    return t;
  }
  (void)uptr;
  (void)sptr;

  const double hz = static_cast<double>(ProcessInfo::clockTicksPerSecond());
  t.userSeconds = static_cast<double>(utime) / hz;
  t.systemSeconds = static_cast<double>(stime) / hz;
  return t;
}

} // namespace muduo::net::inspect

namespace muduo::net {

const string ProcessInspector::username_ = ProcessInfo::username();

void ProcessInspector::registerCommands(Inspector *ins) {
  ins->add("proc", "overview", ProcessInspector::overview, "print basic overview");
  ins->add("proc", "pid", ProcessInspector::pid, "print pid");
  ins->add("proc", "status", ProcessInspector::procStatus,
           "print /proc/self/status");
  ins->add("proc", "threads", ProcessInspector::threads,
           "list /proc/self/task");
}

string ProcessInspector::overview(HttpRequest::Method, const Inspector::ArgList &) {
  using namespace muduo::net::inspect;
  string result;
  result.reserve(1024);

  const Timestamp now = Timestamp::now();
  const Timestamp startedAt = ProcessInfo::startTime();
  result += std::format("Page generated at {} (UTC)\nStarted at {} (UTC), up for {}\n",
                        now.toFormattedString(),
                        startedAt.toFormattedString(),
                        uptime(now, startedAt, true));

  const string procStatusText = ProcessInfo::procStatus();
  result += std::format("{} ({}) running as {} on {}\n",
                        ProcessInfo::procname(procStatusText),
                        ProcessInfo::exePath(),
                        username_,
                        ProcessInfo::hostname());

  if (ProcessInfo::isDebugBuild()) {
    result += "WARNING: debug build!\n";
  }

  result += std::format("pid {}, num of threads {}, bits {}\n",
                        ProcessInfo::pid(),
                        getLong(procStatusText, "Threads:"),
                        CHAR_BIT * sizeof(void *));
  result += std::format("Virtual memory: {:.3f} MiB, RSS memory: {:.3f} MiB\n",
                        static_cast<double>(getLong(procStatusText, "VmSize:")) / 1024.0,
                        static_cast<double>(getLong(procStatusText, "VmRSS:")) / 1024.0);
  result += std::format("Opened files: {}, limit: {}\n",
                        ProcessInfo::openedFiles(),
                        ProcessInfo::maxOpenFiles());

  const auto cpu = ProcessInfo::cpuTime();
  result += std::format("User time: {:12.3f}s\nSys time:  {:12.3f}s\n",
                        cpu.userSeconds,
                        cpu.systemSeconds);
  return result;
}

string ProcessInspector::pid(HttpRequest::Method, const Inspector::ArgList &) {
  return ProcessInfo::pidString();
}

string ProcessInspector::procStatus(HttpRequest::Method,
                                    const Inspector::ArgList &) {
  return ProcessInfo::procStatus();
}

string ProcessInspector::openedFiles(HttpRequest::Method,
                                     const Inspector::ArgList &) {
  return std::to_string(ProcessInfo::openedFiles());
}

string ProcessInspector::threads(HttpRequest::Method, const Inspector::ArgList &) {
  using namespace muduo::net::inspect;

  const auto threads = ProcessInfo::threads();
  string result = "  TID NAME             S    User Time  System Time\n";
  result.reserve(threads.size() * 64);

  for (const pid_t tid : threads) {
    const auto filename =
        std::format("/proc/{}/task/{}/stat", ProcessInfo::pid(), tid);

    string stat;
    if (FileUtil::readFile(filename, 65536, &stat) != 0) {
      continue;
    }

    const std::string_view name = ProcessInfo::procname(stat);
    const size_t rightParen = stat.rfind(')');
    if (rightParen == string::npos || rightParen + 2 >= stat.size()) {
      continue;
    }
    const char state = stat[rightParen + 2];

    std::string_view data(stat);
    data.remove_prefix(rightParen + 4);
    const auto cpu = getCpuTime(data);

    const auto shown = name.substr(0, 16);
    result += std::format("{:5d} {:<16} {} {:12.3f} {:12.3f}\n", tid, shown,
                          state, cpu.userSeconds, cpu.systemSeconds);
  }

  return result;
}

} // namespace muduo::net
