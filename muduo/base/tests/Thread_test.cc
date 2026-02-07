#include "muduo/base/CurrentThread.h"
#include "muduo/base/Thread.h"

#include <gtest/gtest.h>

#include <atomic>

TEST(Thread, StartJoinAndTid) {
  std::atomic<int> workerTid{0};
  const int before = muduo::Thread::numCreated();

  muduo::Thread thread([&] {
    workerTid.store(muduo::CurrentThread::tid(), std::memory_order_release);
  });

  EXPECT_FALSE(thread.started());
  thread.start();
  EXPECT_TRUE(thread.started());
  EXPECT_GT(thread.tid(), 0);
  thread.join();
  EXPECT_TRUE(thread.joined());

  EXPECT_GT(workerTid.load(std::memory_order_acquire), 0);
  EXPECT_GE(muduo::Thread::numCreated(), before + 1);
}

namespace {
void plainFunction() {}
}

TEST(Thread, DefaultNameAndFunctionPointer) {
  muduo::Thread thread(&plainFunction);
  EXPECT_FALSE(thread.name().empty());
  thread.start();
  thread.join();
  EXPECT_TRUE(thread.joined());
}
