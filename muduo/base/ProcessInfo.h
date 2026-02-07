#pragma once

#include "muduo/base/Timestamp.h"
#include "muduo/base/Types.h"

#include <sys/types.h>

#include <string>
#include <string_view>
#include <vector>

namespace muduo::ProcessInfo {

[[nodiscard]] pid_t pid();
[[nodiscard]] string pidString();
[[nodiscard]] uid_t uid();
[[nodiscard]] string username();
[[nodiscard]] uid_t euid();
[[nodiscard]] Timestamp startTime();
[[nodiscard]] int clockTicksPerSecond();
[[nodiscard]] int pageSize();
[[nodiscard]] bool isDebugBuild();

[[nodiscard]] string hostname();
[[nodiscard]] string procname();
[[nodiscard]] std::string_view procname(std::string_view stat);
[[nodiscard]] std::string_view procname(const string &stat);

[[nodiscard]] string procStatus();
[[nodiscard]] string procStat();
[[nodiscard]] string threadStat();

[[nodiscard]] string exePath();

[[nodiscard]] int openedFiles();
[[nodiscard]] int maxOpenFiles();

struct CpuTime {
  double userSeconds{0.0};
  double systemSeconds{0.0};

  [[nodiscard]] double total() const { return userSeconds + systemSeconds; }
};

[[nodiscard]] CpuTime cpuTime();

[[nodiscard]] int numThreads();
[[nodiscard]] std::vector<pid_t> threads();

} // namespace muduo::ProcessInfo
