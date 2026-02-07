#include "muduo/base/CurrentThread.h"

#include <gtest/gtest.h>

#include <sys/wait.h>
#include <unistd.h>

namespace {

thread_local int t_localValue = 0;

} // namespace

TEST(Fork, CurrentThreadTidReCachesInChild) {
  const int parentTid = muduo::CurrentThread::tid();
  ASSERT_EQ(parentTid, static_cast<int>(::getpid()));

  const pid_t child = ::fork();
  ASSERT_NE(child, -1);

  if (child == 0) {
    const int childTid = muduo::CurrentThread::tid();
    ::_exit(childTid == static_cast<int>(::getpid()) ? 0 : 1);
  }

  int status = 0;
  ASSERT_EQ(::waitpid(child, &status, 0), child);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(Fork, ThreadLocalValueIsProcessLocalAfterFork) {
  t_localValue = 1;
  const pid_t child = ::fork();
  ASSERT_NE(child, -1);

  if (child == 0) {
    if (t_localValue != 1) {
      ::_exit(2);
    }
    t_localValue = 2;
    ::_exit(t_localValue == 2 ? 0 : 3);
  }

  int status = 0;
  ASSERT_EQ(::waitpid(child, &status, 0), child);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
  EXPECT_EQ(t_localValue, 1);
}
