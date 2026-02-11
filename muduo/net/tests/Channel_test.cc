#include "muduo/net/Channel.h"

#include "muduo/net/EventLoop.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <utility>

class ChannelTest : public ::testing::Test {};

class PeriodicTimer {
public:
  template <typename F>
    requires std::invocable<std::decay_t<F> &>
  PeriodicTimer(muduo::net::EventLoop *loop,
                std::chrono::microseconds interval, F &&cb)
      : loop_(loop),
        timerfd_(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
        timerChannel_(loop, timerfd_),
        callback_(muduo::detail::MoveOnlyFunction<void()>(std::forward<F>(cb))) {
    if (timerfd_ < 0) {
      return;
    }
    timerChannel_.setReadCallback([this](muduo::Timestamp) { handleRead(); });
    timerChannel_.enableReading();
    active_ = true;
    reset(interval);
  }

  ~PeriodicTimer() {
    if (active_) {
      timerChannel_.disableAll();
      timerChannel_.remove();
    }
    if (timerfd_ >= 0) {
      ::close(timerfd_);
    }
  }

  [[nodiscard]] int fd() const { return timerfd_; }

private:
  void reset(std::chrono::microseconds interval) {
    itimerspec spec{};
    const auto sec = std::chrono::duration_cast<std::chrono::seconds>(interval);
    const auto nsec =
        std::chrono::duration_cast<std::chrono::nanoseconds>(interval - sec);
    spec.it_interval.tv_sec = static_cast<time_t>(sec.count());
    spec.it_interval.tv_nsec = static_cast<long>(nsec.count());
    spec.it_value.tv_sec = static_cast<time_t>(sec.count());
    spec.it_value.tv_nsec = static_cast<long>(nsec.count());
    (void)::timerfd_settime(timerfd_, 0, &spec, nullptr);
  }

  void handleRead() {
    std::uint64_t ticks = 0;
    const auto n = ::read(timerfd_, &ticks, sizeof(ticks));
    EXPECT_EQ(n, static_cast<ssize_t>(sizeof(ticks)));
    if (callback_) {
      callback_();
    }
  }

  muduo::net::EventLoop *loop_;
  int timerfd_;
  muduo::net::Channel timerChannel_;
  muduo::detail::MoveOnlyFunction<void()> callback_;
  bool active_{false};
};

TEST_F(ChannelTest, ReventsToStringContainsFlags) {
  muduo::net::EventLoop loop;
  const int efd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  ASSERT_GE(efd, 0);

  muduo::net::Channel channel(&loop, efd);
  channel.setRevents(POLLIN | POLLOUT | POLLERR);
  const auto s = channel.reventsToString();
  EXPECT_NE(s.find("IN"), std::string::npos);
  EXPECT_NE(s.find("OUT"), std::string::npos);
  EXPECT_NE(s.find("ERR"), std::string::npos);

  channel.enableReading();
  EXPECT_TRUE(channel.isReading());
  channel.enableWriting();
  EXPECT_TRUE(channel.isWriting());
  channel.disableAll();
  EXPECT_TRUE(channel.isNoneEvent());
  channel.remove();

  ::close(efd);
}

TEST_F(ChannelTest, TimerFdIntegrationWithEventLoop) {
  using namespace std::chrono_literals;

  muduo::net::EventLoop loop;
  const int tfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  ASSERT_GE(tfd, 0);

  muduo::net::Channel channel(&loop, tfd);
  bool fired = false;
  channel.setReadCallback([&](muduo::Timestamp) {
    std::uint64_t ticks = 0;
    const auto n = ::read(tfd, &ticks, sizeof(ticks));
    EXPECT_EQ(n, static_cast<ssize_t>(sizeof(ticks)));
    fired = true;
    loop.quit();
  });
  channel.enableReading();

  itimerspec spec{};
  spec.it_value.tv_nsec = 50 * 1000 * 1000;
  ASSERT_EQ(::timerfd_settime(tfd, 0, &spec, nullptr), 0);

  (void)loop.runAfter(1s, [&loop] { loop.quit(); });
  loop.loop();

  EXPECT_TRUE(fired);
  channel.disableAll();
  channel.remove();
  ::close(tfd);
}

TEST_F(ChannelTest, PeriodicTimerAndRunEveryFireTogether) {
  using namespace std::chrono_literals;

  muduo::net::EventLoop loop;
  std::atomic<int> periodicCount{0};
  std::atomic<int> everyCount{0};

  PeriodicTimer periodic(&loop, 20ms, [&] {
    periodicCount.fetch_add(1, std::memory_order_relaxed);
    if (periodicCount.load(std::memory_order_relaxed) >= 3 &&
        everyCount.load(std::memory_order_relaxed) >= 3) {
      loop.quit();
    }
  });
  ASSERT_GE(periodic.fd(), 0);

  (void)loop.runEvery(20ms, [&] {
    everyCount.fetch_add(1, std::memory_order_relaxed);
    if (periodicCount.load(std::memory_order_relaxed) >= 3 &&
        everyCount.load(std::memory_order_relaxed) >= 3) {
      loop.quit();
    }
  });
  (void)loop.runAfter(1s, [&loop] { loop.quit(); });
  loop.loop();

  EXPECT_GE(periodicCount.load(std::memory_order_relaxed), 3);
  EXPECT_GE(everyCount.load(std::memory_order_relaxed), 3);
}
