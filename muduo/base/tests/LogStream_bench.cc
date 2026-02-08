#include "muduo/base/LogStream.h"

#include <benchmark/benchmark.h>
#include <format>
#include <sstream>
#include <string>

using muduo::LogStream;
namespace {

template <typename T> T benchValue(size_t i) { return static_cast<T>(i); }
template <> void* benchValue<void*>(size_t i) { return reinterpret_cast<void*>(i); }

static void BM_LogStream_Int(benchmark::State& state) {
  LogStream os;
  size_t i = 0;
  for (auto _ : state) {
    (void)_;
    os << benchValue<int>(i++);
    benchmark::DoNotOptimize(os.buffer().data());
    os.resetBuffer();
  }
}

static void BM_LogStream_Int64(benchmark::State& state) {
  LogStream os;
  size_t i = 0;
  for (auto _ : state) {
    (void)_;
    os << benchValue<int64_t>(i++);
    benchmark::DoNotOptimize(os.buffer().data());
    os.resetBuffer();
  }
}

static void BM_LogStream_Double(benchmark::State& state) {
  LogStream os;
  size_t i = 0;
  for (auto _ : state) {
    (void)_;
    os << benchValue<double>(i++);
    benchmark::DoNotOptimize(os.buffer().data());
    os.resetBuffer();
  }
}

static void BM_LogStream_Pointer(benchmark::State& state) {
  LogStream os;
  size_t i = 0;
  for (auto _ : state) {
    (void)_;
    os << benchValue<void*>(i++);
    benchmark::DoNotOptimize(os.buffer().data());
    os.resetBuffer();
  }
}

static void BM_StdFormat_Int(benchmark::State& state) {
  int i = 0;
  for (auto _ : state) {
    (void)_;
    auto s = std::format("{}", i++);
    benchmark::DoNotOptimize(s.data());
  }
}

static void BM_StringStream_Int(benchmark::State& state) {
  std::ostringstream os;
  int i = 0;
  for (auto _ : state) {
    (void)_;
    os << i++;
    benchmark::DoNotOptimize(os.str().data());
    os.seekp(0, std::ios_base::beg);
  }
}

} // namespace

BENCHMARK(BM_LogStream_Int);
BENCHMARK(BM_LogStream_Int64);
BENCHMARK(BM_LogStream_Double);
BENCHMARK(BM_LogStream_Pointer);
BENCHMARK(BM_StdFormat_Int);
BENCHMARK(BM_StringStream_Int);
