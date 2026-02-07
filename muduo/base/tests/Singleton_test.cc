#include "muduo/base/Singleton.h"

#include <gtest/gtest.h>

#include <future>
#include <string>
#include <thread>

namespace {

struct TestObject {
  std::string name;
};

struct TestNoDestroy {
  void no_destroy() {}
  int value = 7;
};

} // namespace

TEST(Singleton, SharedAcrossThreads) {
  auto& mainInstance = muduo::Singleton<TestObject>::instance();
  mainInstance.name = "only one";

  std::promise<TestObject*> ptrPromise;
  std::promise<std::string> namePromise;
  std::jthread worker([&](std::stop_token) {
    auto& instance = muduo::Singleton<TestObject>::instance();
    namePromise.set_value(instance.name);
    instance.name = "changed";
    ptrPromise.set_value(&instance);
  });

  const auto* workerPtr = ptrPromise.get_future().get();
  EXPECT_EQ(workerPtr, &mainInstance);
  EXPECT_EQ(namePromise.get_future().get(), "only one");
  EXPECT_EQ(mainInstance.name, "changed");
}

TEST(Singleton, NoDestroyTypeWorks) {
  auto& instance = muduo::Singleton<TestNoDestroy>::instance();
  EXPECT_EQ(instance.value, 7);
}
