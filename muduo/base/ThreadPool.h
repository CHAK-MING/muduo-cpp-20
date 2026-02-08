#pragma once

#include "muduo/base/Thread.h"
#include "muduo/base/Types.h"
#include "muduo/base/noncopyable.h"

#include <atomic>
#include <climits>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace muduo {

class ThreadPool : noncopyable {
public:
  using Task = detail::MoveOnlyFunction<void()>;

  explicit ThreadPool(string nameArg = string("ThreadPool"));
  ~ThreadPool();

  void setThreadInitCallback(Task cb) { threadInitCallback_ = std::move(cb); }
  template <typename F>
    requires std::invocable<std::decay_t<F> &>
  void setThreadInitCallback(F &&cb) {
    threadInitCallback_ = Task(std::forward<F>(cb));
  }

  void start(int numThreads);
  void stop();
  void setMaxQueueSize(int maxSize);

  [[nodiscard]] const string &name() const { return name_; }
  [[nodiscard]] size_t queueSize() const;

  void run(Task f);
  void run(std::function<void()> f) { run(Task(std::move(f))); }
  template <typename F>
    requires std::invocable<std::decay_t<F>>
  void run(F &&f) {
    run(Task(std::forward<F>(f)));
  }

  template <typename F, typename... Args>
    requires std::invocable<std::decay_t<F>, std::decay_t<Args>...>
  [[nodiscard]] auto submit(F &&f, Args &&...args) -> std::future<
      std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using ReturnType =
        std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
    auto promise = std::make_shared<std::promise<ReturnType>>();
    auto future = promise->get_future();

    Task wrapped([func = std::forward<F>(f),
                  ... capturedArgs = std::forward<Args>(args),
                  promise]() mutable {
      if constexpr (std::is_void_v<ReturnType>) {
        std::invoke(std::move(func), std::move(capturedArgs)...);
        promise->set_value();
      } else {
        promise->set_value(
            std::invoke(std::move(func), std::move(capturedArgs)...));
      }
    });

    if (threads_.empty() || !isRunning()) {
      wrapped();
      return future;
    }

    (void)enqueueTask(std::move(wrapped));
    return future;
  }

private:
  struct alignas(64) WorkerState {
    mutable std::mutex mutex;
    std::deque<Task> queue;
    std::binary_semaphore signal{0};
  };

  [[nodiscard]] bool isRunning() const {
    return running_.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool enqueueTask(Task &&task);
  void enqueueSharded(Task &&task);
  [[nodiscard]] std::optional<Task> popSharded(size_t workerIndex);
  void runInThread(const string &threadName, size_t workerIndex);

  string name_;
  Task threadInitCallback_;
  std::vector<std::unique_ptr<Thread>> threads_;
  std::vector<std::unique_ptr<WorkerState>> workers_;
  std::atomic<size_t> nextWorker_{0};
  std::atomic<size_t> queuedTasks_{0};
  std::atomic<bool> running_{false};
  std::atomic<bool> started_{false};
  size_t maxQueueSize_{0};
  std::atomic<int> waitingProducers_{0};
  std::unique_ptr<std::counting_semaphore<INT_MAX>> queueSlots_;
};

} // namespace muduo
