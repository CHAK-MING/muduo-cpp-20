#pragma once

#include <boost/circular_buffer.hpp>

#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <utility>

namespace muduo {

template <typename T>
  requires std::movable<T>
class BoundedBlockingQueue {
public:
  explicit BoundedBlockingQueue(int maxSize)
      : queue_(static_cast<size_t>(maxSize)) {
    if (maxSize <= 0) {
      throw std::invalid_argument("BoundedBlockingQueue maxSize must be > 0");
    }
  }

  BoundedBlockingQueue(const BoundedBlockingQueue &) = delete;
  BoundedBlockingQueue &operator=(const BoundedBlockingQueue &) = delete;

  void put(const T &x)
    requires std::copy_constructible<T>
  {
    std::unique_lock<std::mutex> lock(mutex_);
    notFull_.wait(lock, [this] { return !queue_.full(); });
    queue_.push_back(x);
    lock.unlock();
    notEmpty_.notify_one();
  }

  void put(T &&x) {
    std::unique_lock<std::mutex> lock(mutex_);
    notFull_.wait(lock, [this] { return !queue_.full(); });
    queue_.push_back(std::move(x));
    lock.unlock();
    notEmpty_.notify_one();
  }

  template <typename... Args> void emplace(Args &&...args) {
    std::unique_lock<std::mutex> lock(mutex_);
    notFull_.wait(lock, [this] { return !queue_.full(); });
    queue_.push_back(T(std::forward<Args>(args)...));
    lock.unlock();
    notEmpty_.notify_one();
  }

  T take() {
    std::unique_lock<std::mutex> lock(mutex_);
    notEmpty_.wait(lock, [this] { return !queue_.empty(); });
    T front(std::move(queue_.front()));
    queue_.pop_front();
    lock.unlock();
    notFull_.notify_one();
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
    lock.unlock();
    notFull_.notify_one();
    return front;
  }

  [[nodiscard]] std::optional<T> try_take() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return std::nullopt;
    }
    T front(std::move(queue_.front()));
    queue_.pop_front();
    notFull_.notify_one();
    return front;
  }

  [[nodiscard]] bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  [[nodiscard]] bool full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.full();
  }

  [[nodiscard]] size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  [[nodiscard]] size_t capacity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.capacity();
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable_any notEmpty_;
  std::condition_variable_any notFull_;
  boost::circular_buffer<T> queue_;
};

} // namespace muduo
