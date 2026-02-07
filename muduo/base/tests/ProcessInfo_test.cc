#include "muduo/base/CurrentThread.h"
#include "muduo/base/ProcessInfo.h"

#include <gtest/gtest.h>

#include <algorithm>

TEST(ProcessInfo, BasicProcessProperties) {
  EXPECT_GT(muduo::ProcessInfo::pid(), 0);
  EXPECT_FALSE(muduo::ProcessInfo::pidString().empty());
  EXPECT_FALSE(muduo::ProcessInfo::hostname().empty());
  EXPECT_FALSE(muduo::ProcessInfo::procStat().empty());
  EXPECT_FALSE(muduo::ProcessInfo::procStatus().empty());
  EXPECT_FALSE(muduo::ProcessInfo::threadStat().empty());
  EXPECT_FALSE(muduo::ProcessInfo::exePath().empty());
  EXPECT_TRUE(muduo::ProcessInfo::startTime().valid());
}

TEST(ProcessInfo, ThreadsAndProcName) {
  const auto stat = muduo::ProcessInfo::procStat();
  EXPECT_FALSE(muduo::ProcessInfo::procname(stat).empty());

  const auto tids = muduo::ProcessInfo::threads();
  EXPECT_GE(static_cast<int>(tids.size()), 1);
  EXPECT_GE(muduo::ProcessInfo::numThreads(), 1);
  EXPECT_TRUE(std::binary_search(tids.begin(), tids.end(),
                                 muduo::CurrentThread::tid()));
}

TEST(ProcessInfo, ResourcesAndCpuTime) {
  EXPECT_GT(muduo::ProcessInfo::openedFiles(), 0);
  EXPECT_GE(muduo::ProcessInfo::maxOpenFiles(), muduo::ProcessInfo::openedFiles());
  EXPECT_GT(muduo::ProcessInfo::clockTicksPerSecond(), 0);
  EXPECT_GT(muduo::ProcessInfo::pageSize(), 0);

  const auto cpu = muduo::ProcessInfo::cpuTime();
  EXPECT_GE(cpu.userSeconds, 0.0);
  EXPECT_GE(cpu.systemSeconds, 0.0);
  EXPECT_GE(cpu.total(), 0.0);
}
