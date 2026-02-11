#include "muduo/base/BlockingQueue.h"
#include "muduo/base/Timestamp.h"

#include <benchmark/benchmark.h>

#include <latch>
#include <memory>
#include <thread>
#include <vector>

namespace {

void runHotPotato(int numThreads, int rounds) {
  using Queue = muduo::BlockingQueue<int>;
  muduo::BlockingQueue<std::pair<int, muduo::Timestamp>> done;

  std::vector<std::unique_ptr<Queue>> queues;
  queues.reserve(static_cast<size_t>(numThreads));
  for (int i = 0; i < numThreads; ++i) {
    queues.emplace_back(std::make_unique<Queue>());
  }

  std::latch ready(numThreads);
  std::vector<std::thread> threads;
  threads.reserve(static_cast<size_t>(numThreads));
  for (int i = 0; i < numThreads; ++i) {
    threads.emplace_back([&, i] {
      ready.count_down();
      ready.wait();

      Queue *input = queues[static_cast<size_t>(i)].get();
      Queue *output =
          queues[static_cast<size_t>((i + 1) % numThreads)].get();
      while (true) {
        const int value = input->take();
        if (value > 0) {
          output->put(value - 1);
          continue;
        }
        if (value == 0) {
          done.put(std::make_pair(i, muduo::Timestamp::now()));
        }
        break;
      }
    });
  }

  queues[0]->put(rounds);
  (void)done.take();
  for (const auto &q : queues) {
    q->put(-1);
  }
  for (auto &thread : threads) {
    thread.join();
  }
}

void BM_BlockingQueueHotPotato(benchmark::State &state) {
  const int numThreads = static_cast<int>(state.range(0));
  constexpr int kRounds = 100'003;
  for (auto _ : state) {
    runHotPotato(numThreads, kRounds);
  }
  state.SetItemsProcessed(
      state.iterations() * static_cast<int64_t>(kRounds));
}

} // namespace

BENCHMARK(BM_BlockingQueueHotPotato)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
