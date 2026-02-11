#include "muduo/net/EventLoop.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/Poller.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/net/TimerQueue.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <span>
#include <sys/eventfd.h>
#include <unistd.h>

namespace muduo::net {

namespace {

thread_local EventLoop *t_loopInThisThread = nullptr;
constexpr int kPollTimeMs = 10'000;

[[nodiscard]] int createEventfd() {
  const int eventFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (eventFd < 0) {
    LOG_SYSFATAL << "EventLoop createEventfd failed";
  }
  return eventFd;
}

class IgnoreSigPipe : muduo::noncopyable {
public:
  IgnoreSigPipe() { ::signal(SIGPIPE, SIG_IGN); }
};

const IgnoreSigPipe g_ignoreSigPipe;

} // namespace

EventLoop *EventLoop::getEventLoopOfCurrentThread() noexcept {
  return t_loopInThisThread;
}

EventLoop::EventLoop()
    : threadId_(muduo::CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      timerQueue_(std::make_unique<TimerQueue>(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(std::make_unique<Channel>(this, wakeupFd_)) {
  LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
  if (t_loopInThisThread != nullptr) {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  } else {
    t_loopInThisThread = this;
  }

  wakeupChannel_->setReadCallback(
      [this](Timestamp receiveTime) { handleRead(receiveTime); });
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << muduo::CurrentThread::tid();
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
  assert(!looping_.load(std::memory_order_relaxed));
  assertInLoopThread();

  looping_.store(true, std::memory_order_release);
  quit_.store(false, std::memory_order_release);
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_.load(std::memory_order_acquire)) {
    activeChannels_.clear();
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
    ++iteration_;

    eventHandling_ = true;
    for (auto *channel : activeChannels_) {
      currentActiveChannel_ = channel;
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = nullptr;
    eventHandling_ = false;
    doPendingFunctors();
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_.store(false, std::memory_order_release);
}

void EventLoop::quit() {
  quit_.store(true, std::memory_order_release);
  if (!isInLoopThread()) {
    wakeup();
  }
}

void EventLoop::runInLoop(Functor cb) {
  if (isInLoopThread()) {
    cb();
    return;
  }
  queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(Functor cb) {
  {
    std::scoped_lock lock(mutex_);
    pendingFunctors_.push_back(std::move(cb));
  }

  if (!isInLoopThread() || callingPendingFunctors_) {
    wakeup();
  }
}

size_t EventLoop::queueSize() const {
  std::scoped_lock lock(mutex_);
  return pendingFunctors_.size();
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb) {
  return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delaySeconds, TimerCallback cb) {
  const auto delay =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::duration<double>(delaySeconds));
  return runAfter(delay, std::move(cb));
}

TimerId EventLoop::runAfter(std::chrono::microseconds delay, TimerCallback cb) {
  const Timestamp::TimePoint when = Timestamp::now().timePoint() + delay;
  return runAt(Timestamp{when}, std::move(cb));
}

TimerId EventLoop::runEvery(double intervalSeconds, TimerCallback cb) {
  const auto interval =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::duration<double>(intervalSeconds));
  return runEvery(interval, std::move(cb));
}

TimerId EventLoop::runEvery(std::chrono::microseconds interval,
                            TimerCallback cb) {
  const Timestamp::TimePoint when = Timestamp::now().timePoint() + interval;
  const double intervalSeconds = std::chrono::duration<double>(interval).count();
  return timerQueue_->addTimer(std::move(cb), Timestamp{when}, intervalSeconds);
}

void EventLoop::cancel(TimerId timerId) { timerQueue_->cancel(timerId); }

void EventLoop::updateChannel(Channel *channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_) {
    assert(currentActiveChannel_ == channel ||
           std::find(activeChannels_.begin(), activeChannels_.end(), channel) ==
               activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) const {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread() const {
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " << muduo::CurrentThread::tid();
}

void EventLoop::assertInLoopThread() const {
  if (!isInLoopThread()) {
    abortNotInLoopThread();
  }
}

bool EventLoop::isInLoopThread() const {
  return threadId_ == muduo::CurrentThread::tid();
}

void EventLoop::wakeup() {
  const std::uint64_t one = 1;
  const auto bytes = std::as_bytes(std::span{&one, 1});
  const ssize_t written = sockets::write(wakeupFd_, bytes);
  if (written != static_cast<ssize_t>(sizeof(one))) {
    LOG_ERROR << "EventLoop::wakeup() writes " << written
              << " bytes instead of 8";
  }
}

void EventLoop::handleRead([[maybe_unused]] Timestamp receiveTime) {
  std::uint64_t one = 0;
  const auto bytes = std::as_writable_bytes(std::span{&one, 1});
  const ssize_t n = sockets::read(wakeupFd_, bytes);
  if (n != static_cast<ssize_t>(sizeof(one))) {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
    std::scoped_lock lock(mutex_);
    functors.swap(pendingFunctors_);
  }

  for (auto &functor : functors) {
    functor();
  }
  callingPendingFunctors_ = false;
}

} // namespace muduo::net
