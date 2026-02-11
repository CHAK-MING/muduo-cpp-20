#include "muduo/net/EventLoopThreadPool.h"

#include "muduo/net/EventLoop.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

class EventLoopThreadPoolTest : public ::testing::Test {};

TEST_F(EventLoopThreadPoolTest, ZeroThreadFallsBackToBaseLoop) {
  muduo::net::EventLoop loop;
  muduo::net::EventLoopThreadPool pool(&loop, "single");
  pool.setThreadNum(0);
  std::atomic<int> initCalled{0};
  pool.start([&](muduo::net::EventLoop *p) {
    EXPECT_EQ(p, &loop);
    initCalled.fetch_add(1, std::memory_order_relaxed);
  });
  EXPECT_EQ(initCalled.load(std::memory_order_relaxed), 1);
  EXPECT_TRUE(pool.started());
  EXPECT_EQ(pool.getNextLoop(), &loop);
  EXPECT_EQ(pool.getNextLoop(), &loop);
  EXPECT_EQ(pool.getLoopForHash(42), &loop);
  const auto all = pool.getAllLoops();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all.front(), &loop);
}

TEST_F(EventLoopThreadPoolTest, OneThreadAlwaysReturnsSameWorkerAndRunsTask) {
  using namespace std::chrono_literals;
  muduo::net::EventLoop loop;
  muduo::net::EventLoopThreadPool pool(&loop, "one");
  pool.setThreadNum(1);
  std::atomic<int> initCalled{0};
  std::atomic<bool> ran{false};
  muduo::net::EventLoop *worker = nullptr;
  pool.start([&](muduo::net::EventLoop *p) {
    ++initCalled;
    worker = p;
  });

  ASSERT_NE(worker, nullptr);
  ASSERT_NE(worker, &loop);
  EXPECT_EQ(initCalled.load(std::memory_order_relaxed), 1);
  EXPECT_EQ(pool.getNextLoop(), worker);
  EXPECT_EQ(pool.getNextLoop(), worker);
  EXPECT_EQ(pool.getNextLoop(), worker);

  worker->runInLoop(muduo::net::EventLoop::Functor(
      [&ran] { ran.store(true, std::memory_order_release); }));
  for (int i = 0; i < 200 && !ran.load(std::memory_order_acquire); ++i) {
    std::this_thread::sleep_for(5ms);
  }
  EXPECT_TRUE(ran.load(std::memory_order_acquire));
}

TEST_F(EventLoopThreadPoolTest, RoundRobinWithThreeThreads) {
  muduo::net::EventLoop loop;
  muduo::net::EventLoopThreadPool pool(&loop, "three");
  pool.setThreadNum(3);
  std::atomic<int> initCalled{0};
  pool.start([&](muduo::net::EventLoop *) { ++initCalled; });
  EXPECT_EQ(initCalled.load(std::memory_order_relaxed), 3);

  auto *l1 = pool.getNextLoop();
  auto *l2 = pool.getNextLoop();
  auto *l3 = pool.getNextLoop();
  auto *l4 = pool.getNextLoop();

  EXPECT_NE(l1, &loop);
  EXPECT_NE(l1, l2);
  EXPECT_NE(l2, l3);
  EXPECT_EQ(l1, l4);
  EXPECT_EQ(pool.getLoopForHash(0), l1);
  const auto all = pool.getAllLoops();
  EXPECT_EQ(all.size(), 3u);
}
