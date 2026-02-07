#include "muduo/base/WeakCallback.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

class Counter {
public:
  void add(int value) { sum_ += value; }
  [[nodiscard]] int sum() const { return sum_; }

private:
  int sum_ = 0;
};

} // namespace

TEST(WeakCallback, InvokesWhenAlive) {
  auto obj = std::make_shared<Counter>();
  auto cb = muduo::makeWeakCallback(obj, &Counter::add);

  cb(3);
  cb(4);

  EXPECT_EQ(obj->sum(), 7);
}

TEST(WeakCallback, NoopAfterDestroyed) {
  auto obj = std::make_shared<Counter>();
  auto cb = muduo::makeWeakCallback(obj, &Counter::add);
  obj.reset();

  cb(42);
  SUCCEED();
}
