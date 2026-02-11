#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstdlib>
#include <string>

#include <unistd.h>

namespace muduo::net {
namespace {

class ScopedEnv {
public:
  ScopedEnv(const char *name, const char *value) : name_(name) {
    const char *old = ::getenv(name_);
    if (old != nullptr) {
      hadOld_ = true;
      oldValue_ = old;
    }
    if (value != nullptr) {
      ::setenv(name_, value, 1);
    } else {
      ::unsetenv(name_);
    }
  }

  ~ScopedEnv() {
    if (hadOld_) {
      ::setenv(name_, oldValue_.c_str(), 1);
    } else {
      ::unsetenv(name_);
    }
  }

private:
  const char *name_;
  bool hadOld_{false};
  std::string oldValue_;
};

struct PipeFd {
  std::array<int, 2> fds{-1, -1};

  PipeFd() { EXPECT_EQ(::pipe(fds.data()), 0); }
  ~PipeFd() {
    if (fds[0] >= 0) {
      ::close(fds[0]);
    }
    if (fds[1] >= 0) {
      ::close(fds[1]);
    }
  }
};

TEST(PollerSelectionTest, PollPollerReadEventPath) {
  ScopedEnv usePoll("MUDUO_USE_POLL", "1");

  EventLoop loop;
  PipeFd pipeFd;
  std::atomic<int> callbacks{0};

  Channel channel(&loop, pipeFd.fds[0]);
  channel.setReadCallback([&loop, &channel, &pipeFd, &callbacks](Timestamp) {
    char byte = 0;
    EXPECT_EQ(::read(pipeFd.fds[0], &byte, 1), 1);
    callbacks.fetch_add(1, std::memory_order_relaxed);
    channel.disableAll();
    channel.remove();
    loop.quit();
  });
  channel.enableReading();

  const char byte = 'p';
  ASSERT_EQ(::write(pipeFd.fds[1], &byte, 1), 1);
  loop.loop();

  EXPECT_EQ(callbacks.load(std::memory_order_relaxed), 1);
}

TEST(PollerSelectionTest, PollPollerRemoveSwapPath) {
  ScopedEnv usePoll("MUDUO_USE_POLL", "1");

  EventLoop loop;
  PipeFd p1;
  PipeFd p2;
  std::atomic<int> callbacks{0};

  Channel c1(&loop, p1.fds[0]);
  Channel c2(&loop, p2.fds[0]);
  c1.setReadCallback([&callbacks, &p1](Timestamp) {
    char byte = 0;
    EXPECT_EQ(::read(p1.fds[0], &byte, 1), 1);
    callbacks.fetch_add(1, std::memory_order_relaxed);
  });
  c2.setReadCallback([&loop, &c2, &callbacks, &p2](Timestamp) {
    char byte = 0;
    EXPECT_EQ(::read(p2.fds[0], &byte, 1), 1);
    callbacks.fetch_add(1, std::memory_order_relaxed);
    c2.disableAll();
    c2.remove();
    loop.quit();
  });

  c1.enableReading();
  c2.enableReading();

  // Remove the first channel while second stays active.
  c1.disableAll();
  c1.remove();

  const char byte = 's';
  ASSERT_EQ(::write(p2.fds[1], &byte, 1), 1);
  loop.loop();

  EXPECT_EQ(callbacks.load(std::memory_order_relaxed), 1);
}

TEST(PollerSelectionTest, EPollDeletedToAddedRoundTrip) {
  ScopedEnv useEpoll("MUDUO_USE_POLL", nullptr);

  EventLoop loop;
  PipeFd pipeFd;
  std::atomic<int> callbacks{0};

  Channel channel(&loop, pipeFd.fds[0]);
  channel.setReadCallback([&loop, &channel, &pipeFd, &callbacks](Timestamp) {
    char byte = 0;
    EXPECT_EQ(::read(pipeFd.fds[0], &byte, 1), 1);
    const int n = callbacks.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n == 1) {
      channel.disableAll();   // kAdded -> kDeleted in EPollPoller
      channel.enableReading(); // kDeleted -> kAdded
      const char next = 'e';
      EXPECT_EQ(::write(pipeFd.fds[1], &next, 1), 1);
      return;
    }
    channel.disableAll();
    channel.remove();
    loop.quit();
  });
  channel.enableReading();

  const char first = 'd';
  ASSERT_EQ(::write(pipeFd.fds[1], &first, 1), 1);
  loop.loop();

  EXPECT_EQ(callbacks.load(std::memory_order_relaxed), 2);
}

} // namespace
} // namespace muduo::net
