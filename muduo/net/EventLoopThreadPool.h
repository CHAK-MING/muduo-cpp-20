#pragma once

#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace muduo::net {

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : muduo::noncopyable {
public:
  using ThreadInitCallback = CallbackFunction<void(EventLoop *)>;

  EventLoopThreadPool(EventLoop *baseLoop, string nameArg);
  ~EventLoopThreadPool();

  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(ThreadInitCallback cb = {});
  template <typename F>
    requires CallbackBindable<F, ThreadInitCallback>
  void start(F &&cb) {
    start(ThreadInitCallback(std::forward<F>(cb)));
  }

  [[nodiscard]] EventLoop *getNextLoop();
  [[nodiscard]] EventLoop *getLoopForHash(size_t hashCode);
  [[nodiscard]] std::vector<EventLoop *> getAllLoops();

  [[nodiscard]] bool started() const noexcept { return started_; }
  [[nodiscard]] const string &name() const noexcept { return name_; }

private:
  EventLoop *baseLoop_;
  string name_;
  bool started_{false};
  int numThreads_{0};
  int next_{0};
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop *> loops_;
};

} // namespace muduo::net
