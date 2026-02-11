#include <benchmark/benchmark.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>

namespace {

class DequeBoundedQueue {
public:
  explicit DequeBoundedQueue(std::size_t capacity) : capacity_(capacity) {}

  void put(int v) {
    std::unique_lock<std::mutex> lock(mutex_);
    notFull_.wait(lock, [this] { return queue_.size() < capacity_; });
    queue_.push_back(v);
    lock.unlock();
    notEmpty_.notify_one();
  }

  int take() {
    std::unique_lock<std::mutex> lock(mutex_);
    notEmpty_.wait(lock, [this] { return !queue_.empty(); });
    int value = queue_.front();
    queue_.pop_front();
    lock.unlock();
    notFull_.notify_one();
    return value;
  }

private:
  std::mutex mutex_;
  std::condition_variable notEmpty_;
  std::condition_variable notFull_;
  std::deque<int> queue_;
  std::size_t capacity_;
};

class RingBoundedQueue {
public:
  explicit RingBoundedQueue(std::size_t capacity)
      : buffer_(capacity), capacity_(capacity) {}

  void put(int v) {
    std::unique_lock<std::mutex> lock(mutex_);
    notFull_.wait(lock, [this] { return size_ < capacity_; });
    buffer_[tail_] = v;
    tail_ = (tail_ + 1) % capacity_;
    ++size_;
    lock.unlock();
    notEmpty_.notify_one();
  }

  int take() {
    std::unique_lock<std::mutex> lock(mutex_);
    notEmpty_.wait(lock, [this] { return size_ > 0; });
    const int value = buffer_[head_];
    head_ = (head_ + 1) % capacity_;
    --size_;
    lock.unlock();
    notFull_.notify_one();
    return value;
  }

private:
  std::mutex mutex_;
  std::condition_variable notEmpty_;
  std::condition_variable notFull_;
  std::vector<int> buffer_;
  std::size_t capacity_;
  std::size_t head_{0};
  std::size_t tail_{0};
  std::size_t size_{0};
};

template <typename Queue>
static void BM_BoundedQueue_PingPong(benchmark::State &state) {
  const int rounds = static_cast<int>(state.range(0));
  const std::size_t capacity = static_cast<std::size_t>(state.range(1));

  for (auto _ : state) {
    Queue queue(capacity);
    std::latch ready(2);
    std::atomic<int> consumed{0};

    std::jthread consumer([&](std::stop_token) {
      ready.count_down();
      ready.wait();
      for (int i = 0; i < rounds; ++i) {
        benchmark::DoNotOptimize(queue.take());
        consumed.fetch_add(1, std::memory_order_relaxed);
      }
    });

    ready.count_down();
    ready.wait();
    for (int i = 0; i < rounds; ++i) {
      queue.put(i);
    }

    consumer.join();
    benchmark::DoNotOptimize(consumed.load(std::memory_order_acquire));
  }

  state.counters["items_per_second"] =
      benchmark::Counter(static_cast<double>(rounds),
                         benchmark::Counter::kIsIterationInvariantRate);
}

} // namespace

BENCHMARK_TEMPLATE(BM_BoundedQueue_PingPong, DequeBoundedQueue)
    ->Args({200000, 1024})
    ->UseRealTime();

BENCHMARK_TEMPLATE(BM_BoundedQueue_PingPong, RingBoundedQueue)
    ->Args({200000, 1024})
    ->UseRealTime();
