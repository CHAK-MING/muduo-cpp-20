#include "muduo/base/BlockingQueue.h"
#include "muduo/base/BoundedBlockingQueue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <memory>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

TEST(BlockingQueue, ProducerConsumerWithJthread) {
  muduo::BlockingQueue<int> queue;
  constexpr int kConsumers = 4;
  constexpr int kItems = 2000;

  std::atomic<int> consumed{0};
  std::latch ready(kConsumers + 1);
  std::vector<std::jthread> consumers;
  consumers.reserve(kConsumers);

  for (int i = 0; i < kConsumers; ++i) {
    consumers.emplace_back([&](std::stop_token) {
      ready.count_down();
      ready.wait();
      while (true) {
        const int value = queue.take();
        if (value < 0) {
          break;
        }
        consumed.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  std::jthread producer([&](std::stop_token) {
    ready.count_down();
    ready.wait();
    for (int i = 0; i < kItems; ++i) {
      queue.put(i);
    }
    for (int i = 0; i < kConsumers; ++i) {
      queue.put(-1);
    }
  });

  producer.join();
  for (auto &consumer : consumers) {
    consumer.join();
  }

  EXPECT_EQ(consumed.load(std::memory_order_relaxed), kItems);
}

TEST(BlockingQueue, TakeRespectsStopToken) {
  muduo::BlockingQueue<int> queue;
  std::atomic<bool> exited{false};

  std::jthread consumer([&](std::stop_token token) {
    auto value = queue.take(token);
    EXPECT_FALSE(value.has_value());
    exited.store(true, std::memory_order_release);
  });

  std::this_thread::sleep_for(20ms);
  consumer.request_stop();
  consumer.join();

  EXPECT_TRUE(exited.load(std::memory_order_acquire));
}

TEST(BoundedBlockingQueue, ProducerConsumerWithJthread) {
  muduo::BoundedBlockingQueue<int> queue(16);
  constexpr int kItems = 1000;
  std::atomic<int> consumed{0};

  std::jthread consumer([&](std::stop_token token) {
    while (true) {
      auto value = queue.take(token);
      if (!value.has_value() || *value < 0) {
        break;
      }
      consumed.fetch_add(1, std::memory_order_relaxed);
    }
  });

  std::jthread producer([&](std::stop_token) {
    for (int i = 0; i < kItems; ++i) {
      queue.put(i);
    }
    queue.put(-1);
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(consumed.load(std::memory_order_relaxed), kItems);
}

TEST(BoundedBlockingQueue, TakeRespectsStopToken) {
  muduo::BoundedBlockingQueue<int> queue(4);

  std::jthread consumer([&](std::stop_token token) {
    auto value = queue.take(token);
    EXPECT_FALSE(value.has_value());
  });

  std::this_thread::sleep_for(20ms);
  consumer.request_stop();
  consumer.join();
}

TEST(BlockingQueue, MoveOnlyUniquePtr) {
  muduo::BlockingQueue<std::unique_ptr<int>> queue;

  std::unique_ptr<int> p(new int(42));
  queue.put(std::move(p));
  EXPECT_EQ(p, nullptr);

  auto out = queue.take();
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(*out, 42);
}

TEST(BoundedBlockingQueue, MoveOnlyUniquePtr) {
  muduo::BoundedBlockingQueue<std::unique_ptr<int>> queue(2);

  std::unique_ptr<int> p(new int(7));
  queue.put(std::move(p));
  EXPECT_EQ(p, nullptr);

  auto out = queue.take();
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(*out, 7);
}

} // namespace
