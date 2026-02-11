#pragma once

#include "muduo/base/Timestamp.h"
#include "muduo/base/Types.h"
#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/TimerId.h"

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace muduo::net {

class Channel;
class Poller;
class TimerQueue;

class EventLoop : muduo::noncopyable {
public:
  using Functor = CallbackFunction<void()>;
  using ChannelList = std::vector<Channel *>;

  EventLoop();
  ~EventLoop();

  void loop();
  void quit();

  [[nodiscard]] Timestamp pollReturnTime() const noexcept {
    return pollReturnTime_;
  }
  [[nodiscard]] std::int64_t iteration() const noexcept { return iteration_; }

  void assertInLoopThread() const;
  [[nodiscard]] bool isInLoopThread() const;
  [[nodiscard]] bool eventHandling() const noexcept { return eventHandling_; }

  void updateChannel(Channel *channel);
  void removeChannel(Channel *channel);
  [[nodiscard]] bool hasChannel(Channel *channel) const;

  void runInLoop(Functor cb);
  template <typename F>
    requires CallbackBindable<F, Functor>
  void runInLoop(F &&cb) {
    runInLoop(Functor(std::forward<F>(cb)));
  }
  void queueInLoop(Functor cb);
  template <typename F>
    requires CallbackBindable<F, Functor>
  void queueInLoop(F &&cb) {
    queueInLoop(Functor(std::forward<F>(cb)));
  }
  [[nodiscard]] size_t queueSize() const;

  [[nodiscard]] TimerId runAt(Timestamp time, TimerCallback cb);
  template <typename F>
    requires CallbackBindable<F, TimerCallback>
  [[nodiscard]] TimerId runAt(Timestamp time, F &&cb) {
    return runAt(time, TimerCallback(std::forward<F>(cb)));
  }
#if MUDUO_ENABLE_LEGACY_COMPAT
  [[nodiscard]] TimerId runAfter(double delaySeconds, TimerCallback cb);
#endif
  [[nodiscard]] TimerId runAfter(std::chrono::microseconds delay,
                                 TimerCallback cb);
  template <typename Rep, typename Period>
  [[nodiscard]] TimerId runAfter(std::chrono::duration<Rep, Period> delay,
                                 TimerCallback cb) {
    return runAfter(
        std::chrono::duration_cast<std::chrono::microseconds>(delay),
        std::move(cb));
  }
#if MUDUO_ENABLE_LEGACY_COMPAT
  template <typename F>
    requires CallbackBindable<F, TimerCallback>
  [[nodiscard]] TimerId runAfter(double delaySeconds, F &&cb) {
    return runAfter(delaySeconds, TimerCallback(std::forward<F>(cb)));
  }
#endif
  template <typename Rep, typename Period, typename F>
    requires CallbackBindable<F, TimerCallback>
  [[nodiscard]] TimerId runAfter(std::chrono::duration<Rep, Period> delay,
                                 F &&cb) {
    return runAfter(delay, TimerCallback(std::forward<F>(cb)));
  }
#if MUDUO_ENABLE_LEGACY_COMPAT
  [[nodiscard]] TimerId runEvery(double intervalSeconds, TimerCallback cb);
#endif
  [[nodiscard]] TimerId runEvery(std::chrono::microseconds interval,
                                 TimerCallback cb);
  template <typename Rep, typename Period>
  [[nodiscard]] TimerId runEvery(std::chrono::duration<Rep, Period> interval,
                                 TimerCallback cb) {
    return runEvery(
        std::chrono::duration_cast<std::chrono::microseconds>(interval),
        std::move(cb));
  }
#if MUDUO_ENABLE_LEGACY_COMPAT
  template <typename F>
    requires CallbackBindable<F, TimerCallback>
  [[nodiscard]] TimerId runEvery(double intervalSeconds, F &&cb) {
    return runEvery(intervalSeconds, TimerCallback(std::forward<F>(cb)));
  }
#endif
  template <typename Rep, typename Period, typename F>
    requires CallbackBindable<F, TimerCallback>
  [[nodiscard]] TimerId runEvery(std::chrono::duration<Rep, Period> interval,
                                 F &&cb) {
    return runEvery(interval, TimerCallback(std::forward<F>(cb)));
  }
  void cancel(TimerId timerId);

  void wakeup() const;

  [[nodiscard]] static EventLoop *getEventLoopOfCurrentThread() noexcept;

private:
  void abortNotInLoopThread() const;
  void handleRead(Timestamp receiveTime);
  void doPendingFunctors();

  std::atomic<bool> looping_{false};
  std::atomic<bool> quit_{false};
  bool eventHandling_{false};
  bool callingPendingFunctors_{false};
  std::int64_t iteration_{0};
  const int threadId_;
  Timestamp pollReturnTime_;
  std::unique_ptr<Poller> poller_;
  std::unique_ptr<TimerQueue> timerQueue_;
  int wakeupFd_{-1};
  std::unique_ptr<Channel> wakeupChannel_;

  ChannelList activeChannels_;
  Channel *currentActiveChannel_{nullptr};

  mutable std::mutex mutex_;
  std::vector<Functor> pendingFunctors_;
};

} // namespace muduo::net
