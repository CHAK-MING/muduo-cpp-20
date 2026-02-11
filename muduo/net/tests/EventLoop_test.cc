#include "muduo/net/EventLoop.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/Thread.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

class EventLoopTest : public ::testing::Test {};

TEST_F(EventLoopTest, CurrentThreadLoopPointer) {
  EXPECT_EQ(muduo::net::EventLoop::getEventLoopOfCurrentThread(), nullptr);
  muduo::net::EventLoop loop;
  EXPECT_EQ(muduo::net::EventLoop::getEventLoopOfCurrentThread(), &loop);
}

TEST_F(EventLoopTest, RunAfterFromAnotherThread) {
  using namespace std::chrono_literals;

  muduo::net::EventLoop loop;
  std::atomic<bool> fired{false};

  muduo::Thread thr([&loop, &fired] {
    (void)loop.runAfter(50ms, [&fired] {
      fired.store(true, std::memory_order_release);
    });
    (void)loop.runAfter(100ms, [&loop] { loop.quit(); });
  });
  thr.start();
  loop.loop();
  thr.join();

  EXPECT_TRUE(fired.load(std::memory_order_acquire));
}

TEST_F(EventLoopTest, ThreadOwnsItsLoopAndCanQuitFromTimer) {
  using namespace std::chrono_literals;

  std::atomic<bool> beforeNull{false};
  std::atomic<bool> duringMatch{false};
  std::atomic<bool> callbackRan{false};

  muduo::Thread thr([&] {
    beforeNull.store(
        muduo::net::EventLoop::getEventLoopOfCurrentThread() == nullptr,
        std::memory_order_release);

    muduo::net::EventLoop threadLoop;
    duringMatch.store(
        muduo::net::EventLoop::getEventLoopOfCurrentThread() == &threadLoop,
        std::memory_order_release);

    (void)threadLoop.runAfter(30ms, [&] {
      callbackRan.store(true, std::memory_order_release);
      threadLoop.quit();
    });
    threadLoop.loop();
  });

  thr.start();
  thr.join();

  EXPECT_TRUE(beforeNull.load(std::memory_order_acquire));
  EXPECT_TRUE(duringMatch.load(std::memory_order_acquire));
  EXPECT_TRUE(callbackRan.load(std::memory_order_acquire));
}

TEST_F(EventLoopTest, ChronoDurationTimerApis) {
  using namespace std::chrono_literals;

  muduo::net::EventLoop loop;
  std::atomic<int> everyCount{0};

  const auto repeating = loop.runEvery(10ms, [&] {
    if (everyCount.fetch_add(1, std::memory_order_relaxed) + 1 >= 3) {
      loop.quit();
    }
  });

  (void)loop.runAfter(200ms, [&] { loop.quit(); });
  loop.loop();
  loop.cancel(repeating);

  EXPECT_GE(everyCount.load(std::memory_order_relaxed), 3);
}

TEST_F(EventLoopTest, QueueInLoopFromManyThreadsNoLoss) {
  using namespace std::chrono_literals;

  muduo::net::EventLoop loop;
  constexpr int kThreads = 8;
  constexpr int kTasksPerThread = 200;
  constexpr int kTotalTasks = kThreads * kTasksPerThread;

  std::atomic<int> executed{0};
  std::latch ready(kThreads);
  std::vector<std::jthread> workers;
  workers.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([&](std::stop_token) {
      ready.count_down();
      ready.wait();
      for (int i = 0; i < kTasksPerThread; ++i) {
        loop.queueInLoop([&] {
          const int done = executed.fetch_add(1, std::memory_order_acq_rel) + 1;
          if (done == kTotalTasks) {
            loop.quit();
          }
        });
      }
    });
  }

  (void)loop.runAfter(2s, [&loop] { loop.quit(); });
  loop.loop();
  for (auto &worker : workers) {
    worker.join();
  }

  EXPECT_EQ(executed.load(std::memory_order_acquire), kTotalTasks);
}

#if GTEST_HAS_DEATH_TEST
TEST_F(EventLoopTest, OneLoopPerThreadDeath) {
  ASSERT_DEATH(
      {
        muduo::net::EventLoop first;
        muduo::net::EventLoop second;
        (void)first;
        (void)second;
      },
      ".*");
}
#endif
