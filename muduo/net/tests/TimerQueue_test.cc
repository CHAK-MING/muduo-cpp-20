#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/base/CurrentThread.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

class TimerQueueTest : public ::testing::Test {};

TEST_F(TimerQueueTest, RunAfterRunEveryAndDoubleCancel) {
  using namespace std::chrono_literals;

  muduo::net::EventLoop loop;
  std::atomic<int> onceCount{0};
  std::atomic<int> every2Count{0};
  std::atomic<int> every3Count{0};
  std::atomic<int> once45Count{0};
  std::atomic<int> cancelCount{0};

  constexpr auto kT02 = 20ms;
  constexpr auto kT03 = 30ms;
  constexpr auto kT05 = 50ms;
  constexpr auto kT07 = 70ms;
  constexpr auto kT085 = 85ms;
  constexpr auto kT09 = 90ms;
  constexpr auto kT095 = 95ms;
  constexpr auto kT18 = 180ms;
  constexpr auto kT30 = 300ms;
  constexpr auto kEvery2 = 20ms;
  constexpr auto kEvery3 = 30ms;

  (void)loop.runAfter(kT02, [&onceCount] {
    onceCount.fetch_add(1, std::memory_order_relaxed);
  });
  (void)loop.runAfter(kT03, [&onceCount] {
    onceCount.fetch_add(1, std::memory_order_relaxed);
  });
  (void)loop.runAfter(kT05, [&onceCount] {
    onceCount.fetch_add(1, std::memory_order_relaxed);
  });
  (void)loop.runAfter(kT07, [&onceCount] {
    onceCount.fetch_add(1, std::memory_order_relaxed);
  });

  muduo::net::TimerId once45 =
      loop.runAfter(kT09, [&once45Count] {
        once45Count.fetch_add(1, std::memory_order_relaxed);
      });

  muduo::net::TimerId every2 =
      loop.runEvery(kEvery2, [&every2Count] {
        every2Count.fetch_add(1, std::memory_order_relaxed);
      });
  muduo::net::TimerId every3 =
      loop.runEvery(kEvery3, [&every3Count] {
        every3Count.fetch_add(1, std::memory_order_relaxed);
      });

  (void)loop.runAfter(kT085, [&] {
    loop.cancel(once45);
    cancelCount.fetch_add(1, std::memory_order_relaxed);
  });
  (void)loop.runAfter(kT095, [&] {
    loop.cancel(once45);
    cancelCount.fetch_add(1, std::memory_order_relaxed);
  });
  (void)loop.runAfter(kT18, [&] {
    loop.cancel(every3);
    cancelCount.fetch_add(1, std::memory_order_relaxed);
  });
  (void)loop.runAfter(kT30, [&loop] { loop.quit(); });

  loop.loop();

  EXPECT_EQ(onceCount.load(std::memory_order_relaxed), 4);
  EXPECT_EQ(once45Count.load(std::memory_order_relaxed), 0);
  EXPECT_GE(every2Count.load(std::memory_order_relaxed), 8);
  EXPECT_GE(every3Count.load(std::memory_order_relaxed), 4);
  EXPECT_LE(every3Count.load(std::memory_order_relaxed), 9);
  EXPECT_EQ(cancelCount.load(std::memory_order_relaxed), 3);
}

TEST_F(TimerQueueTest, RunAfterOnEventLoopThread) {
  using namespace std::chrono_literals;

  muduo::net::EventLoopThread loopThread;
  muduo::net::EventLoop *loop = loopThread.startLoop();
  ASSERT_NE(loop, nullptr);

  std::atomic<bool> fired{false};
  (void)loop->runAfter(30ms, [&fired] {
    fired.store(true, std::memory_order_release);
  });

  for (int i = 0; i < 100 && !fired.load(std::memory_order_acquire); ++i) {
    muduo::CurrentThread::sleepUsec(5 * 1000);
  }
  EXPECT_TRUE(fired.load(std::memory_order_acquire));
}

TEST_F(TimerQueueTest, OrderedEventsAndCancelBehavior) {
  using namespace std::chrono_literals;

  muduo::net::EventLoop loop;
  std::vector<std::string> events;
  muduo::net::TimerId repeating;

  (void)loop.runAfter(10ms, [&] { events.emplace_back("once1"); });
  (void)loop.runAfter(15ms, [&] { events.emplace_back("once1.5"); });
  (void)loop.runAfter(25ms, [&] { events.emplace_back("once2.5"); });

  repeating = loop.runEvery(10ms, [&] {
    events.emplace_back("every");
  });
  (void)loop.runAfter(45ms, [&] {
    loop.cancel(repeating);
    events.emplace_back("cancel");
  });
  (void)loop.runAfter(90ms, [&loop] { loop.quit(); });

  loop.loop();

  const auto pos = [&](std::string_view token) {
    for (size_t i = 0; i < events.size(); ++i) {
      if (events[i] == token) {
        return i;
      }
    }
    return events.size();
  };

  const size_t p1 = pos("once1");
  const size_t p15 = pos("once1.5");
  const size_t p25 = pos("once2.5");
  const size_t pc = pos("cancel");
  ASSERT_LT(p1, events.size());
  ASSERT_LT(p15, events.size());
  ASSERT_LT(p25, events.size());
  ASSERT_LT(pc, events.size());
  EXPECT_LT(p1, p15);
  EXPECT_LT(p15, p25);

  int everyBeforeCancel = 0;
  int everyAfterCancel = 0;
  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i] != "every") {
      continue;
    }
    if (i < pc) {
      ++everyBeforeCancel;
    } else {
      ++everyAfterCancel;
    }
  }
  EXPECT_GE(everyBeforeCancel, 2);
  EXPECT_EQ(everyAfterCancel, 0);
}

TEST_F(TimerQueueTest, RunAfterHighConcurrencyNoLoss) {
  using namespace std::chrono_literals;

  muduo::net::EventLoop loop;
  constexpr int kThreads = 8;
  constexpr int kTasksPerThread = 120;
  constexpr int kTotalTasks = kThreads * kTasksPerThread;

  std::atomic<int> fired{0};
  std::latch ready(kThreads);
  std::vector<std::jthread> workers;
  workers.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([&](std::stop_token) {
      ready.count_down();
      ready.wait();
      for (int i = 0; i < kTasksPerThread; ++i) {
        (void)loop.runAfter(1ms, [&] {
          fired.fetch_add(1, std::memory_order_acq_rel);
        });
      }
    });
  }

  (void)loop.runAfter(250ms, [&loop] { loop.quit(); });
  loop.loop();
  for (auto &worker : workers) {
    worker.join();
  }

  EXPECT_EQ(fired.load(std::memory_order_acquire), kTotalTasks);
}
