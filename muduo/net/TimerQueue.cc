#include "muduo/net/TimerQueue.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Types.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Timer.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <ranges>
#include <sys/timerfd.h>
#include <unistd.h>

namespace muduo::net::detail {

int createTimerfd() {
  const int timerfd =
      ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0) {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

[[nodiscard]] timespec howMuchTimeFromNow(Timestamp when) {
  std::int64_t microseconds =
      when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
  microseconds = std::max<std::int64_t>(microseconds, 100);

  timespec ts{};
  ts.tv_sec =
      static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<int64_t>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}

void readTimerfd(int timerfd, Timestamp now) {
  std::uint64_t howmany = 0;
  const ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at "
            << now.toString();
  if (n != static_cast<ssize_t>(sizeof howmany)) {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of "
              << sizeof(howmany);
  }
}

void resetTimerfd(int timerfd, Timestamp expiration) {
  itimerspec newValue{};
  itimerspec oldValue{};
  newValue.it_value = howMuchTimeFromNow(expiration);

  if (::timerfd_settime(timerfd, 0, &newValue, &oldValue) != 0) {
    LOG_SYSERR << "timerfd_settime()";
  }
}

} // namespace muduo::net::detail

using namespace muduo;
using namespace muduo::net;

TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop), timerfd_(detail::createTimerfd()),
      timerfdChannel_(loop, timerfd_), callingExpiredTimers_(false) {
  timerfdChannel_.setReadCallback(
      [this]([[maybe_unused]] Timestamp ts) { handleRead(ts); });
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
  timerfdChannel_.disableAll();
  timerfdChannel_.remove();
  ::close(timerfd_);
}

TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when,
                             double interval) {
  auto timer = std::make_unique<Timer>(std::move(cb), when, interval);
  const auto sequence = timer->sequence();
  loop_->runInLoop([this, timer = std::move(timer)]() mutable {
    addTimerInLoop(std::move(timer));
  });
  return TimerId{sequence};
}

void TimerQueue::cancel(TimerId timerId) {
  loop_->runInLoop([this, timerId] { cancelInLoop(timerId); });
}

void TimerQueue::addTimerInLoop(std::unique_ptr<Timer> timer) {
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  const auto timerSeq = timer->sequence();
  const Timestamp expiration = timer->expiration();
  const bool earliestChanged = insertTimer(*timer);
  const auto [ownerIt, ownerInserted] =
      timerOwners_.emplace(timerSeq, std::move(timer));
  (void)ownerIt;
  assert(ownerInserted);
  if (earliestChanged) {
    detail::resetTimerfd(timerfd_, expiration);
  }
}

void TimerQueue::cancelInLoop(TimerId timerId) {
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  if (!timerId.valid()) {
    return;
  }

  const ActiveTimer timer = timerId.sequence();
  const auto it = activeTimers_.find(timer);
  if (it != activeTimers_.end()) {
    const auto ownerIt = timerOwners_.find(*it);
    assert(ownerIt != timerOwners_.end());
    const size_t erased =
        timers_.erase(Entry(ownerIt->second->expiration(), *it));
    assert(erased == 1);
    [[maybe_unused]] const auto ownersErased = timerOwners_.erase(*it);
    assert(ownersErased == 1);
    activeTimers_.erase(it);
  } else if (callingExpiredTimers_) {
    cancelingTimers_.insert(timer);
  }

  assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead([[maybe_unused]] Timestamp receiveTime) {
  loop_->assertInLoopThread();

  const Timestamp now(Timestamp::now());
  detail::readTimerfd(timerfd_, now);
  auto expired = getExpired(now);

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();
  std::ranges::for_each(expired, [this](const Entry &entry) {
    auto ownerIt = timerOwners_.find(entry.second);
    if (ownerIt != timerOwners_.end()) {
      ownerIt->second->run();
    }
  });
  callingExpiredTimers_ = false;

  reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now) {
  assert(timers_.size() == activeTimers_.size());

  std::vector<Entry> expired;
  const auto sentry = Entry(now, std::numeric_limits<TimerSequence>::max());
  const auto end = timers_.upper_bound(sentry);
  expired.reserve(static_cast<size_t>(
      std::ranges::distance(std::ranges::begin(timers_), end)));

  const auto first = std::ranges::begin(timers_);
  std::ranges::copy(std::ranges::subrange(first, end),
                    std::back_inserter(expired));
  timers_.erase(first, end);

  std::ranges::for_each(expired, [this](const Entry &entry) {
    [[maybe_unused]] const size_t erased = activeTimers_.erase(entry.second);
    assert(erased == 1);
  });

  assert(timers_.size() == activeTimers_.size());
  return expired;
}

void TimerQueue::reset(std::span<const Entry> expired, Timestamp now) {
  Timestamp nextExpire;

  std::ranges::for_each(expired, [this, now](const Entry &entry) {
    const auto timerSeq = entry.second;
    auto ownerIt = timerOwners_.find(timerSeq);
    if (ownerIt == timerOwners_.end()) {
      return;
    }

    auto &timer = *ownerIt->second;
    if (timer.repeat() && !cancelingTimers_.contains(timerSeq)) {
      timer.restart(now);
      (void)insertTimer(timer);
    } else {
      [[maybe_unused]] const auto ownersErased = timerOwners_.erase(timerSeq);
      assert(ownersErased == 1);
    }
  });

  if (!timers_.empty()) {
    nextExpire = std::ranges::begin(timers_)->first;
  }

  if (nextExpire.valid()) {
    detail::resetTimerfd(timerfd_, nextExpire);
  }
}

bool TimerQueue::insertTimer(Timer &timer) {
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());

  bool earliestChanged = false;
  const Timestamp when = timer.expiration();
  const TimerSequence seq = timer.sequence();
  if (timers_.empty() || when < std::ranges::begin(timers_)->first) {
    earliestChanged = true;
  }

  const auto [timerIt, timerInserted] = timers_.insert(Entry(when, seq));
  (void)timerIt;
  assert(timerInserted);

  const auto [activeIt, activeInserted] = activeTimers_.insert(seq);
  (void)activeIt;
  assert(activeInserted);

  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}
