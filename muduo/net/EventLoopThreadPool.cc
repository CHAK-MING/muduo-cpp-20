#include "muduo/net/EventLoopThreadPool.h"

#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"

#include <cassert>
#include <format>
#include <memory>

namespace muduo::net {

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, string nameArg)
    : baseLoop_(baseLoop), name_(std::move(nameArg)) {}

EventLoopThreadPool::~EventLoopThreadPool() = default;

void EventLoopThreadPool::start(ThreadInitCallback cb) {
  assert(!started_);
  baseLoop_->assertInLoopThread();

  started_ = true;
  threads_.reserve(static_cast<size_t>(numThreads_));
  loops_.reserve(static_cast<size_t>(numThreads_));
  auto sharedInitCb = std::make_shared<ThreadInitCallback>(std::move(cb));

  for (int i = 0; i < numThreads_; ++i) {
    auto threadName = std::format("{}{}", name_, i);
    auto t = std::make_unique<EventLoopThread>(
        EventLoopThread::ThreadInitCallback{[sharedInitCb](EventLoop *loop) {
          if (sharedInitCb != nullptr && static_cast<bool>(*sharedInitCb)) {
            (*sharedInitCb)(loop);
          }
        }},
        std::move(threadName));
    loops_.push_back(t->startLoop());
    threads_.push_back(std::move(t));
  }

  if (numThreads_ == 0 && sharedInitCb != nullptr &&
      static_cast<bool>(*sharedInitCb)) {
    (*sharedInitCb)(baseLoop_);
  }
}

EventLoop *EventLoopThreadPool::getNextLoop() {
  baseLoop_->assertInLoopThread();
  assert(started_);

  EventLoop *loop = baseLoop_;
  if (!loops_.empty()) {
    loop = loops_[static_cast<size_t>(next_)];
    ++next_;
    if (static_cast<size_t>(next_) >= loops_.size()) {
      next_ = 0;
    }
  }
  return loop;
}

EventLoop *EventLoopThreadPool::getLoopForHash(size_t hashCode) {
  baseLoop_->assertInLoopThread();
  assert(started_);
  if (loops_.empty()) {
    return baseLoop_;
  }
  return loops_[hashCode % loops_.size()];
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops() {
  baseLoop_->assertInLoopThread();
  assert(started_);
  if (loops_.empty()) {
    return {baseLoop_};
  }
  return loops_;
}

} // namespace muduo::net
