#include "muduo/base/ThreadPool.h"

#include <benchmark/benchmark.h>
#include <atomic>
#include <latch>

namespace {

static void BM_ThreadPool_Run(benchmark::State& state) {
  const int workers = static_cast<int>(state.range(0));
  const int maxQueue = static_cast<int>(state.range(1));
  muduo::ThreadPool pool("benchPool");
  pool.setMaxQueueSize(maxQueue);
  pool.start(workers);

  for (auto _ : state) {
    (void)_;
    std::atomic<int64_t> done{0};
    const int tasks = static_cast<int>(state.range(2));
    std::latch allDone(tasks);

    for (int i = 0; i < tasks; ++i) {
      pool.run([&] {
        done.fetch_add(1, std::memory_order_relaxed);
        allDone.count_down();
      });
    }
    allDone.wait();
    benchmark::DoNotOptimize(done.load(std::memory_order_acquire));
  }

  pool.stop();
  state.counters["tasks_per_second"] =
      benchmark::Counter(static_cast<double>(state.range(2)),
                         benchmark::Counter::kIsIterationInvariantRate);
}

} // namespace

BENCHMARK(BM_ThreadPool_Run)
    ->Args({1, 0, 200000})
    ->Args({2, 0, 200000})
    ->Args({4, 0, 200000})
    ->Args({8, 0, 200000})
    ->Args({16, 0, 200000})
    ->Args({1, 1024, 200000})
    ->Args({2, 1024, 200000})
    ->Args({4, 1024, 200000})
    ->Args({8, 1024, 200000})
    ->Args({16, 1024, 200000})
    ->UseRealTime();
