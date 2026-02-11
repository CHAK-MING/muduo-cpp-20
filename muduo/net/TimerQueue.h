#pragma once

#include "muduo/base/Timestamp.h"
#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/Channel.h"
#include "muduo/net/TimerId.h"

#include <chrono>
#include <concepts>
#include <set>
#include <span>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace muduo::net {

class EventLoop;
class Timer;

class TimerQueue : muduo::noncopyable {
public:
  explicit TimerQueue(EventLoop *loop);
  ~TimerQueue();

  [[nodiscard]] TimerId addTimer(TimerCallback cb, Timestamp when,
                                 std::chrono::microseconds interval);
  template <typename F>
    requires CallbackBindable<F, TimerCallback>
  [[nodiscard]] TimerId addTimer(F &&cb, Timestamp when,
                                 std::chrono::microseconds interval) {
    return addTimer(TimerCallback(std::forward<F>(cb)), when, interval);
  }
  void cancel(TimerId timerId);

private:
  using TimerSequence = std::int64_t;
  using Entry = std::pair<Timestamp, TimerSequence>;
  using TimerList = std::set<Entry>;
  using ActiveTimer = TimerSequence;
  using ActiveTimerSet = std::set<ActiveTimer>;
  using TimerOwnerMap = std::unordered_map<TimerSequence, std::unique_ptr<Timer>>;

  void addTimerInLoop(std::unique_ptr<Timer> timer);
  void cancelInLoop(TimerId timerId);
  void handleRead(Timestamp receiveTime);
  [[nodiscard]] std::vector<Entry> getExpired(Timestamp now);
  void reset(std::span<const Entry> expired, Timestamp now);

  [[nodiscard]] bool insertTimer(Timer &timer);

  EventLoop *loop_;
  const int timerfd_;
  Channel timerfdChannel_;
  TimerList timers_;

  ActiveTimerSet activeTimers_;
  bool callingExpiredTimers_;
  ActiveTimerSet cancelingTimers_;
  TimerOwnerMap timerOwners_;
};

} // namespace muduo::net
