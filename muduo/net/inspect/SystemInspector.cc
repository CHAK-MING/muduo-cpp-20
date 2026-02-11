#include "muduo/net/inspect/SystemInspector.h"

#include "muduo/base/FileUtil.h"
#include "muduo/net/inspect/ProcessInspector.h"

#include <format>
#include <string_view>
#include <sys/utsname.h>

namespace muduo::net {
using namespace std::string_view_literals;

void SystemInspector::registerCommands(Inspector *ins) {
  ins->add("sys", "overview", SystemInspector::overview, "print system overview");
  ins->add("sys", "loadavg", SystemInspector::loadavg, "print /proc/loadavg");
  ins->add("sys", "version", SystemInspector::version, "print /proc/version");
  ins->add("sys", "cpuinfo", SystemInspector::cpuinfo, "print /proc/cpuinfo");
  ins->add("sys", "meminfo", SystemInspector::meminfo, "print /proc/meminfo");
  ins->add("sys", "stat", SystemInspector::stat, "print /proc/stat");
}

string SystemInspector::loadavg(HttpRequest::Method, const Inspector::ArgList &) {
  string data;
  FileUtil::readFile("/proc/loadavg"sv, 65536, &data);
  return data;
}

string SystemInspector::version(HttpRequest::Method, const Inspector::ArgList &) {
  string data;
  FileUtil::readFile("/proc/version"sv, 65536, &data);
  return data;
}

string SystemInspector::cpuinfo(HttpRequest::Method, const Inspector::ArgList &) {
  string data;
  FileUtil::readFile("/proc/cpuinfo"sv, 65536, &data);
  return data;
}

string SystemInspector::meminfo(HttpRequest::Method, const Inspector::ArgList &) {
  string data;
  FileUtil::readFile("/proc/meminfo"sv, 65536, &data);
  return data;
}

string SystemInspector::stat(HttpRequest::Method, const Inspector::ArgList &) {
  string data;
  FileUtil::readFile("/proc/stat"sv, 65536, &data);
  return data;
}

string SystemInspector::overview(HttpRequest::Method, const Inspector::ArgList &) {
  using namespace muduo::net::inspect;

  string result;
  result.reserve(1024);

  const Timestamp now = Timestamp::now();
  result += "Page generated at ";
  result += now.toFormattedString();
  result += " (UTC)\n";

  struct utsname un {};
  if (::uname(&un) == 0) {
    result += std::format("Hostname: {}\n", un.nodename);
    result += std::format("Machine: {}\n", un.machine);
    result += std::format("OS: {} {} {}\n", un.sysname, un.release, un.version);
  }

  string statText;
  FileUtil::readFile("/proc/stat"sv, 65536, &statText);
  const Timestamp bootTime(Timestamp::kMicroSecondsPerSecond *
                           getLong(statText, "btime "));
  result += "Boot time: ";
  result += bootTime.toFormattedString(false);
  result += " (UTC)\n";
  result += "Up time: ";
  result += uptime(now, bootTime, false);
  result += "\n";

  string loadavgText;
  FileUtil::readFile("/proc/loadavg"sv, 65536, &loadavgText);
  result += std::format("Processes created: {}\n",
                        getLong(statText, "processes "));
  result += std::format("Loadavg: {}\n", loadavgText);

  string meminfoText;
  FileUtil::readFile("/proc/meminfo"sv, 65536, &meminfoText);
  const long totalKb = getLong(meminfoText, "MemTotal:");
  const long freeKb = getLong(meminfoText, "MemFree:");
  const long buffersKb = getLong(meminfoText, "Buffers:");
  const long cachedKb = getLong(meminfoText, "Cached:");

  result += std::format("Total Memory: {:6} MiB\n", totalKb / 1024);
  result += std::format("Free Memory:  {:6} MiB\n", freeKb / 1024);
  result += std::format("Buffers:      {:6} MiB\n", buffersKb / 1024);
  result += std::format("Cached:       {:6} MiB\n", cachedKb / 1024);
  result += std::format("Real Used:    {:6} MiB\n",
                        (totalKb - freeKb - buffersKb - cachedKb) / 1024);
  result += std::format("Real Free:    {:6} MiB\n",
                        (freeKb + buffersKb + cachedKb) / 1024);

  return result;
}

} // namespace muduo::net
