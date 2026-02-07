#pragma once

#include <concepts>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace muduo {

template <typename T>
concept QueueElement = std::movable<T>;

template <QueueElement T> class BlockingQueue {
public:
  BlockingQueue() = default;

  BlockingQueue(const BlockingQueue &) = delete;
  BlockingQueue &operator=(const BlockingQueue &) = delete;

  void put(const T &x)
    requires std::copy_constructible<T>
  {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.emplace_back(x);
    }
    notEmpty_.notify_one();
  }

  void put(T &&x) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.emplace_back(std::move(x));
    }
    notEmpty_.notify_one();
  }

  template <typename... Args> void emplace(Args &&...args) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.emplace_back(std::forward<Args>(args)...);
    }
    notEmpty_.notify_one();
  }

  T take() {
    std::unique_lock<std::mutex> lock(mutex_);
    notEmpty_.wait(lock, [this] { return !queue_.empty(); });

    T front(std::move(queue_.front()));
    queue_.pop_front();
    return front;
  }

  [[nodiscard]] std::optional<T> take(std::stop_token stopToken) {
    std::unique_lock<std::mutex> lock(mutex_);
    notEmpty_.wait(lock, stopToken, [this] { return !queue_.empty(); });
    if (queue_.empty()) {
      return std::nullopt;
    }
    T front(std::move(queue_.front()));
    queue_.pop_front();
    return front;
  }

  [[nodiscard]] std::optional<T> try_take() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return std::nullopt;
    }
    T front(std::move(queue_.front()));
    queue_.pop_front();
    return front;
  }

  std::deque<T> drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::deque<T> out = std::move(queue_);
    queue_.clear();
    return out;
  }

  [[nodiscard]] size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  [[nodiscard]] bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable_any notEmpty_;
  std::deque<T> queue_;
};

} // namespace muduo
