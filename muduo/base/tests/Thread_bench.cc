#include "muduo/base/Thread.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <memory>
#include <vector>

namespace {

static void BM_Thread_CreateJoin_Serial(benchmark::State& state) {
  for (auto _ : state) {
    (void)_;
    muduo::Thread thread([] {});
    thread.start();
    thread.join();
  }
}

static void BM_Thread_CreateJoin_ParallelBatch(benchmark::State& state) {
  const int batch = static_cast<int>(state.range(0));

  for (auto _ : state) {
    (void)_;
    std::atomic<int> done{0};
    std::vector<std::unique_ptr<muduo::Thread>> threads;
    threads.reserve(static_cast<size_t>(batch));

    for (int i = 0; i < batch; ++i) {
      threads.emplace_back(std::make_unique<muduo::Thread>(
          [&] { done.fetch_add(1, std::memory_order_relaxed); }));
    }
    for (auto& thread : threads) {
      thread->start();
    }
    for (auto& thread : threads) {
      thread->join();
    }

    benchmark::DoNotOptimize(done.load(std::memory_order_acquire));
  }

  state.counters["threads_per_second"] =
      benchmark::Counter(static_cast<double>(batch),
                         benchmark::Counter::kIsIterationInvariantRate);
}

} // namespace

BENCHMARK(BM_Thread_CreateJoin_Serial)->UseRealTime();
BENCHMARK(BM_Thread_CreateJoin_ParallelBatch)->Arg(128)->UseRealTime();
