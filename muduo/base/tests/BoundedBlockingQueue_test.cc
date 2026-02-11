#include "muduo/base/BoundedBlockingQueue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace {

using namespace std::chrono_literals;

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

TEST(BoundedBlockingQueue, MoveOnlyUniquePtr) {
  muduo::BoundedBlockingQueue<std::unique_ptr<int>> queue(2);

  auto p = std::make_unique<int>(7);
  queue.put(std::move(p));
  EXPECT_EQ(p, nullptr);

  auto out = queue.take();
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(*out, 7);
}

} // namespace
