#pragma once

#include "muduo/base/noncopyable.h"

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

template <QueueElement T> class BlockingQueue : noncopyable {
public:
  BlockingQueue() = default;

  void put(const T &x)
    requires std::copy_constructible<T>
  {
    {
      std::scoped_lock lock(mutex_);
      queue_.emplace_back(x);
    }
    notEmpty_.notify_one();
  }

  void put(T &&x) {
    {
      std::scoped_lock lock(mutex_);
      queue_.emplace_back(std::move(x));
    }
    notEmpty_.notify_one();
  }

  template <typename... Args> void emplace(Args &&...args) {
    {
      std::scoped_lock lock(mutex_);
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
    std::scoped_lock lock(mutex_);
    if (queue_.empty()) {
      return std::nullopt;
    }
    T front(std::move(queue_.front()));
    queue_.pop_front();
    return front;
  }

  std::deque<T> drain() {
    std::scoped_lock lock(mutex_);
    std::deque<T> out = std::move(queue_);
    queue_.clear();
    return out;
  }

  [[nodiscard]] size_t size() const {
    std::scoped_lock lock(mutex_);
    return queue_.size();
  }

  [[nodiscard]] bool empty() const {
    std::scoped_lock lock(mutex_);
    return queue_.empty();
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable_any notEmpty_;
  std::deque<T> queue_;
};

} // namespace muduo
