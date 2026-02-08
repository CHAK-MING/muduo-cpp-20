#include "muduo/base/BlockingQueue.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <latch>
#include <thread>

namespace {

static void BM_BlockingQueue_PingPong(benchmark::State& state) {
  const int rounds = static_cast<int>(state.range(0));

  for (auto _ : state) {
    (void)_;
    muduo::BlockingQueue<int> queue;
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

BENCHMARK(BM_BlockingQueue_PingPong)->Arg(200000)->UseRealTime();
