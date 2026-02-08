#include "muduo/base/ThreadPool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <semaphore>
#include <string>
#include <thread>
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

TEST(ThreadPool, SubmitReturnsFuture) {
  muduo::ThreadPool pool("submitPool");
  pool.start(4);

  auto sumFuture = pool.submit([](int a, int b) { return a + b; }, 11, 31);
  auto voidFuture = pool.submit([]() {});
  auto stringFuture = pool.submit(
      [](std::string prefix, int value) { return prefix + std::to_string(value); },
      std::string("id-"), 42);

  EXPECT_EQ(sumFuture.get(), 42);
  voidFuture.get();
  EXPECT_EQ(stringFuture.get(), "id-42");

  pool.stop();
}

TEST(ThreadPool, RunAfterStopDoesNotExecute) {
  muduo::ThreadPool pool("stopPool");
  pool.start(2);
  pool.stop();

  std::atomic<int> count{0};
  pool.run([&] { count.fetch_add(1, std::memory_order_relaxed); });
  EXPECT_EQ(count.load(std::memory_order_relaxed), 0);
}

TEST(ThreadPool, MaxQueueSizeBlocksProducer) {
  using namespace std::chrono_literals;

  muduo::ThreadPool pool("boundedPool");
  pool.setMaxQueueSize(1);
  pool.start(1);

  std::binary_semaphore gate{0};
  std::atomic<bool> producerEntered{false};
  std::atomic<bool> producerReturned{false};
  std::atomic<bool> ran2{false};
  std::atomic<bool> ran3{false};

  pool.run([&] { gate.acquire(); });
  pool.run([&] { ran2.store(true, std::memory_order_release); });

  std::jthread producer([&](std::stop_token) {
    producerEntered.store(true, std::memory_order_release);
    pool.run([&] { ran3.store(true, std::memory_order_release); });
    producerReturned.store(true, std::memory_order_release);
  });

  std::this_thread::sleep_for(50ms);
  EXPECT_TRUE(producerEntered.load(std::memory_order_acquire));
  EXPECT_FALSE(producerReturned.load(std::memory_order_acquire));

  gate.release();

  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (std::chrono::steady_clock::now() < deadline &&
         (!producerReturned.load(std::memory_order_acquire) ||
          !ran3.load(std::memory_order_acquire))) {
    std::this_thread::sleep_for(1ms);
  }

  EXPECT_TRUE(ran2.load(std::memory_order_acquire));
  EXPECT_TRUE(producerReturned.load(std::memory_order_acquire));
  EXPECT_TRUE(ran3.load(std::memory_order_acquire));

  pool.stop();
}
