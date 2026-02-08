#include "muduo/base/CurrentThread.h"
#include "muduo/base/Thread.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <vector>

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

TEST(Thread, DestructorAfterStartJoinsWorker) {
  std::atomic<bool> done{false};
  {
    muduo::Thread thread([&] { done.store(true, std::memory_order_release); });
    thread.start();
  }
  EXPECT_TRUE(done.load(std::memory_order_acquire));
}

TEST(Thread, DestroyStartedThreadsInContainer) {
  constexpr int kThreads = 8;
  std::atomic<int> done{0};
  std::vector<std::unique_ptr<muduo::Thread>> threads;
  threads.reserve(kThreads);

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back(std::make_unique<muduo::Thread>([&] {
      done.fetch_add(1, std::memory_order_relaxed);
    }));
  }
  for (auto& thread : threads) {
    thread->start();
  }

  threads.clear();
  EXPECT_EQ(done.load(std::memory_order_acquire), kThreads);
}
