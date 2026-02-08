# muduo-cpp-20

`muduo-cpp-20` is a C++20 modernization of the **muduo project**.
It keeps familiar public APIs for easier migration, while rewriting internals with modern C++20 features and concurrency primitives.

The goal is practical: preserve existing usage patterns where possible, reduce legacy complexity inside, and improve throughput on modern hardware.

## Build

### Requirements

- CMake >= 3.16
- C++20 compiler
  - GCC >= 13
  - Clang >= 18
- Boost >= 1.85 (`system`)
- Zlib

### Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Test

Original muduo tests have been rewritten with GoogleTest, and additional coverage was added for modernized code paths.

Run all tests:

```bash
ctest --test-dir build --output-on-failure
```

## Compatibility and Usage

### For Existing muduo Projects

Most legacy APIs are still available, but some C++11-era wrapper components were intentionally not kept as full standalone layers (for example, old `Mutex.h` / `Atomic.h` style wrappers).

Legacy logging macros are **enabled by default**.

- If you want old macro-style logging, do not define `MUDUO_DISABLE_LEGACY_LOG_MACROS`.

### For New C++20 Projects

Prefer modern logging APIs and disable legacy macro paths:

- Define `MUDUO_DISABLE_LEGACY_LOG_MACROS=1`

Example:

```cpp
muduo::logWarn("cpu={} load={:.2f}", 16, 0.73);
```

## Removed Wrappers and Replacements

To avoid redundant abstractions in C++20 code, several old wrappers were removed or reduced:

- `CountDownLatch.h` -> `std::latch`
- `Mutex.h` and related lock wrappers -> `std::mutex`, `std::condition_variable_any`
- Legacy string adapter usage -> `std::string_view` as the default model

Compatibility aliases are retained where practical, but new code should use standard C++20 types directly.

## Benchmarks (Measured)

Measured on the same host/session.

| Case | Baseline (old muduo) | muduo-cpp-20 | Gain |
|---|---:|---:|---:|
| Logging (`nop`, single-thread hot path) | 5,026,211.69 msg/s | ~11,337,868 msg/s (`88.2 ns/op`) | **2.26x** |
| ThreadPool (`8 workers`, `200000 tasks`, unbounded queue) | 245,041.58 tasks/s | 624,368 tasks/s | **2.55x** |

Notes:
- Logging baseline comes from old `logging_test` (`nop` line).
- New logging result comes from `logging_bench` (`BM_Logging_Stream`).
- ThreadPool baseline was measured with an old-muduo-compatible micro benchmark on the same machine.

### Benchmark Commands

```bash
./build/muduo/base/tests/logging_bench --benchmark_min_time=0.3s
./build/muduo/base/tests/threadpool_bench --benchmark_min_time=0.3s
```

## License

MIT License
