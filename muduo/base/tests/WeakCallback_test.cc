#include "muduo/base/WeakCallback.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>

namespace {

class Counter {
public:
  void add(int value) {
    sum_ += value;
    callCount_.fetch_add(1, std::memory_order_relaxed);
  }
  [[nodiscard]] int sum() const { return sum_; }
  [[nodiscard]] static int callCount() {
    return callCount_.load(std::memory_order_relaxed);
  }

private:
  static std::atomic<int> callCount_;
  int sum_ = 0;
};

std::atomic<int> Counter::callCount_{0};

} // namespace

TEST(WeakCallback, InvokesWhenAlive) {
  auto obj = std::make_shared<Counter>();
  auto cb = muduo::makeWeakCallback(obj, &Counter::add);

  cb(3);
  cb(4);

  EXPECT_EQ(obj->sum(), 7);
}

TEST(WeakCallback, NoopAfterDestroyed) {
  const int before = Counter::callCount();
  auto obj = std::make_shared<Counter>();
  auto cb = muduo::makeWeakCallback(obj, &Counter::add);
  obj.reset();

  cb(42);
  EXPECT_EQ(Counter::callCount(), before);
}
