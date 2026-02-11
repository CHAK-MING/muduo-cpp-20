#pragma once

#include "muduo/base/Thread.h"
#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <mutex>
#include <type_traits>
#include <utility>

namespace muduo::net {

class EventLoop;

class EventLoopThread : muduo::noncopyable {
public:
  using ThreadInitCallback = CallbackFunction<void(EventLoop *)>;

  explicit EventLoopThread(ThreadInitCallback cb = {}, string name = {});
  template <typename F>
    requires CallbackBindable<F, ThreadInitCallback>
  explicit EventLoopThread(F &&cb, string name = {})
      : EventLoopThread(ThreadInitCallback(std::forward<F>(cb)),
                        std::move(name)) {}
  ~EventLoopThread();

  [[nodiscard]] EventLoop *startLoop();

private:
  void threadFunc();

  EventLoop *loop_{nullptr};
  std::atomic<bool> exiting_{false};
  muduo::Thread thread_;
  std::mutex mutex_;
  std::condition_variable cond_;
  ThreadInitCallback callback_;
};

} // namespace muduo::net
