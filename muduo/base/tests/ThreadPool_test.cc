#include "muduo/base/ThreadPool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <latch>

TEST(ThreadPool, RunTasksUnboundedQueue) {
  muduo::ThreadPool pool("testPool");
  pool.start(4);

  constexpr int kTasks = 200;
  std::atomic<int> count{0};
  std::latch done(kTasks);

  for (int i = 0; i < kTasks; ++i) {
    pool.run([&] {
      count.fetch_add(1, std::memory_order_relaxed);
      done.count_down();
    });
  }

  done.wait();
  pool.stop();
  EXPECT_EQ(count.load(std::memory_order_relaxed), kTasks);
}

TEST(ThreadPool, RunTasksBoundedQueue) {
  muduo::ThreadPool pool("boundedPool");
  pool.setMaxQueueSize(8);
  pool.start(3);

  constexpr int kTasks = 120;
  std::atomic<int> count{0};
  std::latch done(kTasks);

  for (int i = 0; i < kTasks; ++i) {
    pool.run([&] {
      count.fetch_add(1, std::memory_order_relaxed);
      done.count_down();
    });
  }

  done.wait();
  pool.stop();
  EXPECT_EQ(count.load(std::memory_order_relaxed), kTasks);
}

TEST(ThreadPool, RunInCallerThreadWhenNoWorkers) {
  muduo::ThreadPool pool("inlinePool");
  pool.start(0);

  std::atomic<int> count{0};
  pool.run([&] { count.fetch_add(1, std::memory_order_relaxed); });
  pool.stop();
  EXPECT_EQ(count.load(std::memory_order_relaxed), 1);
}

TEST(ThreadPool, ThreadInitCallbackRunsPerWorker) {
  muduo::ThreadPool pool("initPool");
  std::atomic<int> initCount{0};
  pool.setThreadInitCallback([&] { initCount.fetch_add(1, std::memory_order_relaxed); });

  pool.start(3);
  pool.stop();
  EXPECT_EQ(initCount.load(std::memory_order_relaxed), 3);
}
