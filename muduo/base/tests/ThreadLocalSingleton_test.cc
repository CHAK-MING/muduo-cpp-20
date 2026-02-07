#include "muduo/base/ThreadLocalSingleton.h"

#include <gtest/gtest.h>

#include <array>
#include <future>
#include <string>
#include <thread>

namespace {

struct TestObject {
  std::string name;
};

} // namespace

TEST(ThreadLocalSingleton, DistinctPerThread) {
  auto& mainInstance = muduo::ThreadLocalSingleton<TestObject>::instance();
  mainInstance.name = "main";

  std::array<std::future<TestObject*>, 2> ptrFuture;
  std::array<std::future<std::string>, 2> nameFuture;
  std::array<std::promise<TestObject*>, 2> ptrPromise;
  std::array<std::promise<std::string>, 2> namePromise;
  std::array<std::jthread, 2> workers;

  for (int i = 0; i < 2; ++i) {
    ptrFuture[i] = ptrPromise[i].get_future();
    nameFuture[i] = namePromise[i].get_future();
    workers[i] = std::jthread([&, i](std::stop_token) {
      auto& instance = muduo::ThreadLocalSingleton<TestObject>::instance();
      namePromise[i].set_value(instance.name);
      instance.name = (i == 0) ? "thread1" : "thread2";
      ptrPromise[i].set_value(&instance);
    });
  }

  auto* ptr0 = ptrFuture[0].get();
  auto* ptr1 = ptrFuture[1].get();
  EXPECT_NE(ptr0, &mainInstance);
  EXPECT_NE(ptr1, &mainInstance);
  EXPECT_NE(ptr0, ptr1);
  EXPECT_TRUE(nameFuture[0].get().empty());
  EXPECT_TRUE(nameFuture[1].get().empty());
  EXPECT_EQ(mainInstance.name, "main");
}
