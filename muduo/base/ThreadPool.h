#pragma once

#include "muduo/base/BlockingQueue.h"
#include "muduo/base/BoundedBlockingQueue.h"
#include "muduo/base/Types.h"

#include <atomic>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace muduo {

class ThreadPool {
public:
  using Task = detail::MoveOnlyFunction;

  explicit ThreadPool(string nameArg = string("ThreadPool"));
  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  void setMaxQueueSize(int maxSize) { maxQueueSize_ = maxSize; }
  template <typename F>
    requires std::invocable<std::decay_t<F> &>
  void setThreadInitCallback(F &&cb) {
    threadInitCallback_ = Task(std::forward<F>(cb));
  }

  void start(int numThreads);
  void stop();

  [[nodiscard]] const string &name() const { return name_; }
  [[nodiscard]] size_t queueSize() const;

  void run(Task f);
  template <typename F>
    requires std::invocable<std::decay_t<F>>
  void run(F &&f) {
    if (threads_.empty()) {
      std::invoke(std::forward<F>(f));
      return;
    }
    if (!isRunning()) {
      return;
    }
    put(Task(std::forward<F>(f)));
  }

private:
  [[nodiscard]] bool isRunning() const {
    return running_.load(std::memory_order_acquire);
  }
  [[nodiscard]] std::optional<Task> take(std::stop_token stopToken);
  void put(Task &&task);
  void runInThread(std::stop_token stopToken, const string &threadName);

  string name_;
  Task threadInitCallback_;
  std::vector<std::jthread> threads_;
  std::unique_ptr<BlockingQueue<Task>> queue_;
  std::unique_ptr<BoundedBlockingQueue<Task>> boundedQueue_;
  size_t maxQueueSize_ = 0;
  std::atomic<bool> running_{false};
};

} // namespace muduo
