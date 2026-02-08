#include "muduo/base/Singleton.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <future>
#include <string>
#include <thread>
#include <vector>

namespace {

struct TestObject {
  std::string name;
};

struct TestNoDestroy {
  void no_destroy() {}
  int value = 7;
};

std::atomic<int> g_ctorCount{0};

struct ContendedInitObject {
  ContendedInitObject() {
    g_ctorCount.fetch_add(1, std::memory_order_relaxed);
  }
  int payload = 42;
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

TEST(Singleton, InstanceInitializationRaceWith64Threads) {
  g_ctorCount.store(0, std::memory_order_release);
  constexpr size_t kThreads = 64;
  std::array<ContendedInitObject*, kThreads> addresses{};
  std::vector<std::jthread> workers;
  workers.reserve(kThreads);

  for (size_t i = 0; i < kThreads; ++i) {
    workers.emplace_back([i, &addresses](std::stop_token) {
      addresses.at(i) = &muduo::Singleton<ContendedInitObject>::instance();
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }

  const auto* first = addresses.front();
  ASSERT_NE(first, nullptr);
  for (const auto* addr : addresses) {
    EXPECT_EQ(addr, first);
  }
  EXPECT_EQ(first->payload, 42);
  EXPECT_EQ(g_ctorCount.load(std::memory_order_acquire), 1);
}
