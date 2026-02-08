# muduo-cpp-20

`muduo-cpp-20` is an in-progress C++20 modernization of the whole `muduo` project. The current phase delivers substantial upgrades in core components (base), with `muduo/net` modernization as the next major milestone.

## Highlights

- Modern C++20 core:
  - `std::jthread`, `std::stop_token`
  - `std::binary_semaphore`, `std::latch`
  - `std::source_location`, `std::format`
- High-throughput async logging:
  - shard front-end buffers to reduce producer contention
  - backend recycling pool to reduce allocation churn

## Performance (Measured)

Measured on this repository with current binaries on **2026-02-08** (Release-like benchmark binaries, 5-run average).

| Scenario | Baseline | muduo-cpp-20 | Gain |
| :-- | --: | --: | --: |
| Sync logging (`nop`) | 5,218,971 msg/s | 9,226,340 msg/s | +76.8% |
| Async logging (`16 x 20,000`) | 1,217,465 msg/s | 6,596,865 msg/s | +441.9% |

Only measured numbers are kept here. Temporary benchmark files are not part of the project.

## Benchmark Environment

- OS: `Linux 6.17.0-8-generic x86_64`
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

## Removed Headers and Replacements

Legacy utility wrappers are intentionally reduced where C++20 standard facilities are clearer and faster.

Removed or no longer provided as dedicated wrappers:

- `muduo/base/CountDownLatch.h`
- `muduo/base/noncopyable.h`
- `muduo/base/copyable.h`
- `muduo/base/Mutex.h` and related lock-guard wrapper variants
- `muduo/base/Condition.h` wrapper variants

Recommended replacements:

- Latch/synchronization: `std::latch`, `std::counting_semaphore`, `std::mutex`, `std::condition_variable_any`
- Copy control: deleted copy/move operations in-class
- String view adapters: prefer `std::string_view` for new code; `StringPiece` / `StringArg` compatibility aliases remain available and can be disabled via `MUDUO_DISABLE_LEGACY_LOG_MACROS`
- Threading: `std::jthread`, `std::stop_token`
- Paths: `std::filesystem::path`

## Roadmap (Next: `muduo/net`)

- Rebuild event loop core with C++20 primitives while preserving muduo-compatible public APIs.
- Migrate channel/poller/timer internals to modern type-safe abstractions.
- Introduce clearer stop/lifecycle semantics for I/O worker threads (`std::jthread` + `std::stop_token`).
- Expand benchmark coverage for net stack latency/throughput and publish measured results.
