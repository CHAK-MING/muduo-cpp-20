#include "muduo/base/Logging.h"

#include <benchmark/benchmark.h>
#include <atomic>

namespace {

std::atomic<int64_t> g_totalBytes{0};

void dummyOutput(const char *msg, int len) {
  (void)msg;
  g_totalBytes.fetch_add(len, std::memory_order_relaxed);
}

void dummyFlush() {}

void benchSetup() {
  g_totalBytes.store(0, std::memory_order_release);
  muduo::Logger::setOutput(dummyOutput);
  muduo::Logger::setFlush(dummyFlush);
  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::INFO);
}

static void BM_Logging_Stream(benchmark::State &state) {
  benchSetup();
  int i = 0;
  for (auto _ : state) {
    (void)_;
    LOG_INFO << "Hello 0123456789 abcdefghijklmnopqrstuvwxyz " << i;
    ++i;
  }
  const auto bytes = static_cast<double>(g_totalBytes.load(std::memory_order_acquire));
  state.counters["bytes_per_msg"] = bytes / static_cast<double>(state.iterations());
  state.counters["throughput_mib_s"] =
      benchmark::Counter(bytes, benchmark::Counter::kIsRate,
                         benchmark::Counter::OneK::kIs1024) /
      (1024.0 * 1024.0);
}

} // namespace

BENCHMARK(BM_Logging_Stream)->UseRealTime()->Threads(1);
