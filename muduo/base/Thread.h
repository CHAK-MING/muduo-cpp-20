#pragma once

#include "muduo/base/Types.h"
#include "muduo/base/noncopyable.h"

#include <atomic>
#include <concepts>
#include <string>
#include <sys/types.h>
#include <thread>

namespace muduo {

class Thread : noncopyable {
public:
  using ThreadFunc = detail::MoveOnlyFunction<void()>;

  template <typename F>
    requires(!std::same_as<std::remove_cvref_t<F>, Thread> &&
             std::invocable<std::decay_t<F> &>)
  explicit Thread(F &&func, string name = string{})
      : func_(std::forward<F>(func)), name_(std::move(name)) {
    setDefaultName();
  }

  void start();
  int join();

  [[nodiscard]] bool started() const {
    return tid_.load(std::memory_order_acquire) > 0;
  }
  [[nodiscard]] bool joined() const { return started() && !thread_.joinable(); }
  [[nodiscard]] pid_t tid() const {
    return tid_.load(std::memory_order_acquire);
  }
  [[nodiscard]] const string &name() const { return name_; }

  [[nodiscard]] static int numCreated() { return numCreated_.load(); }

private:
  void setDefaultName();
  void runInThread();

  std::jthread thread_;
  std::atomic<pid_t> tid_{0};
  ThreadFunc func_;
  string name_;

  static std::atomic<int> numCreated_;
};

} // namespace muduo
