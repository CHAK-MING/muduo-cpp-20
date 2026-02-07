#include "muduo/base/CurrentThread.h"

#include <gtest/gtest.h>

#include <future>
#include <string>
#include <thread>
#include <chrono>

TEST(CurrentThread, BasicPropertiesInMainThread) {
  EXPECT_GT(muduo::CurrentThread::tid(), 0);
  EXPECT_TRUE(muduo::CurrentThread::isMainThread());
  EXPECT_GT(muduo::CurrentThread::tidStringLength(), 0);
  EXPECT_EQ(muduo::CurrentThread::tidStringView().size(),
            static_cast<size_t>(muduo::CurrentThread::tidStringLength()));
}

TEST(CurrentThread, NameAndMainThreadFlagInWorker) {
  std::promise<bool> isMainPromise;
  std::promise<std::string> namePromise;

  std::jthread worker([&](std::stop_token) {
    muduo::CurrentThread::setName("worker-test");
    isMainPromise.set_value(muduo::CurrentThread::isMainThread());
    namePromise.set_value(std::string(muduo::CurrentThread::name()));
  });

  EXPECT_FALSE(isMainPromise.get_future().get());
  EXPECT_EQ(namePromise.get_future().get(), "worker-test");
}

TEST(CurrentThread, SleepUsecAndStackTrace) {
  const auto before = std::chrono::steady_clock::now();
  muduo::CurrentThread::sleepUsec(5'000);
  const auto elapsed = std::chrono::steady_clock::now() - before;
  EXPECT_GE(elapsed, std::chrono::microseconds(4'000));

  const auto trace = muduo::CurrentThread::stackTrace(false);
  EXPECT_FALSE(trace.empty());
}
