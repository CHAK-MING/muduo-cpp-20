# muduo-cpp-20

`muduo-cpp-20` is an in-progress C++20 modernization of the whole `muduo` project. The current phase delivers substantial upgrades in core components (especially logging/base), with `muduo/net` modernization as the next major milestone.

## Highlights

- Modern C++20 core:
  - `std::jthread`, `std::stop_token`
  - `std::binary_semaphore`, `std::latch`
  - `std::source_location`, `std::format`
- High-throughput async logging:
  - 8-shard front-end buffers to reduce producer contention
  - backend recycling pool to reduce allocation churn
- Logging API supports two styles:
  - chained stream style: `muduo::logInfo() << ...`
  - format style: `muduo::logInfo("x={} y={}", x, y)`

## Performance (Measured)

Measured on this repository with current binaries on **2026-02-07** (Release-like benchmark binaries).

| Scenario | Baseline (old) | muduo-cpp-20 (new) | Gain |
| :-- | --: | --: | --: |
| Sync logging (`nop`) | 5,173,930 msg/s | 9,020,338 msg/s | +74.3% |
| Async logging (`16 x 20,000`) | 1,147,430 msg/s | 6,589,516 msg/s | +474.2% |

Only measured numbers are kept here. Temporary benchmark files are not part of the project.

## Benchmark Environment

- OS: `Linux pm-server 6.17.0-8-generic x86_64`
- CPU: `Intel(R) Xeon(R) Gold 5218 CPU @ 2.30GHz`
- Logical CPUs: `32`
- Compiler (GCC): `g++ 15.2.0`
- Compiler (Clang): `clang++ 20.1.8`
- CMake: `3.31.6`
- Boost: `1.88` (`BOOST_LIB_VERSION "1_88"`)

## Build

### Requirements

- CMake >= 3.16
- C++20 compiler:
  - GCC >= 13, or
  - Clang >= 18
- Boost >= 1.85 (`system`)

### Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Test

Run all GoogleTest cases:

```bash
ctest --test-dir build --output-on-failure
```

## Usage

```cpp
#include "muduo/base/Logging.h"

int main() {
  muduo::logInfo() << "Connection established from " << "127.0.0.1" << ":" << 8080;
  muduo::logWarn("cpu={} load={:.2f}", 16, 0.73);
}
```

## Migration Notes (Old Muduo -> C++20)

Old muduo commonly uses macros:

```cpp
LOG_INFO << "Connection established from " << "127.0.0.1" << ":" << 8080;
```

In this repository, the direct replacement is:

```cpp
muduo::logInfo() << "Connection established from " << "127.0.0.1" << ":" << 8080;
```

This keeps the stream style with minimal code changes, while new projects are recommended to use format-style logging where appropriate:

```cpp
muduo::logWarn("cpu={} load={:.2f}", 16, 0.73);
```

## Removed Headers and Replacements

Some legacy utility headers were intentionally removed where C++20 standard facilities are clearer and faster:

- Removed: `muduo/base/CountDownLatch.h`
  - Use: `std::latch`
- Removed: `muduo/base/noncopyable.h`
  - Use: deleted special members (`T(const T&) = delete; T& operator=(const T&) = delete;`)
- Removed: macro-centric logging entry points as the primary API
  - Use: function-based logging APIs (`muduo::logInfo()`, `muduo::logWarn(...)`, etc.)

## Roadmap (Next: `muduo/net`)

- Rebuild event loop core with C++20 primitives while preserving muduo-compatible public APIs.
- Migrate channel/poller/timer internals to modern type-safe abstractions.
- Introduce clearer stop/lifecycle semantics for I/O worker threads (`std::jthread` + `std::stop_token`).
- Expand benchmark coverage for net stack latency/throughput and publish measured results.
