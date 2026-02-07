#pragma once

#include "muduo/base/Types.h"

#include <atomic>
#include <concepts>
#include <string>
#include <sys/types.h>
#include <thread>

namespace muduo {

class Thread {
public:
  using ThreadFunc = detail::MoveOnlyFunction;

  template <typename F>
    requires(!std::same_as<std::remove_cvref_t<F>, Thread> &&
             std::invocable<std::decay_t<F> &>)
  explicit Thread(F &&func, string name = string{})
      : func_(std::forward<F>(func)), name_(std::move(name)) {
    setDefaultName();
  }
  ~Thread();

  Thread(const Thread &) = delete;
  Thread &operator=(const Thread &) = delete;

  void start();
  int join();

  [[nodiscard]] bool started() const { return started_; }
  [[nodiscard]] bool joined() const { return started_ && !thread_.joinable(); }
  [[nodiscard]] pid_t tid() const { return tid_; }
  [[nodiscard]] const string &name() const { return name_; }

  [[nodiscard]] static int numCreated() { return numCreated_.load(); }

private:
  void setDefaultName();
  void runInThread();

  bool started_ = false;
  std::jthread thread_;
  std::atomic<pid_t> tid_{0};
  ThreadFunc func_;
  string name_;

  static std::atomic<int> numCreated_;
};

} // namespace muduo
