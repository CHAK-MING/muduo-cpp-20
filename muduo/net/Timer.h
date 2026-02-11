#pragma once

#include "muduo/base/Timestamp.h"
#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace muduo::net {

class Timer : muduo::noncopyable {
public:
  Timer(TimerCallback cb, Timestamp when, std::chrono::microseconds interval)
      : callback_(std::move(cb)), expiration_(when), interval_(interval),
        repeat_(interval > std::chrono::microseconds::zero()),
        sequence_(s_numCreated_.fetch_add(1, std::memory_order_relaxed) + 1) {}

  void run() { callback_(); }

  [[nodiscard]] Timestamp expiration() const { return expiration_; }
  [[nodiscard]] bool repeat() const { return repeat_; }
  [[nodiscard]] std::int64_t sequence() const { return sequence_; }

  void restart(Timestamp now);

  [[nodiscard]] static std::int64_t numCreated() {
    return s_numCreated_.load(std::memory_order_relaxed);
  }

private:
  TimerCallback callback_;
  Timestamp expiration_;
  const std::chrono::microseconds interval_;
  const bool repeat_;
  const std::int64_t sequence_;

  static std::atomic<std::int64_t> s_numCreated_;
};

} // namespace muduo::net
