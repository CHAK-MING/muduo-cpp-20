# muduo-cpp-20

**Version:** 0.1.0

`muduo-cpp-20` is a C++20 modernization of [muduo], a high-performance C++ network library based on the Reactor model.

This project focuses on production-ready engineering.
- Preserve muduo-style network programming semantics.
- Modernize internals with C++20 features.
- Maintain compatibility with existing muduo projects.

## Quick Start

Minimal echo server setup.

```cpp
muduo::net::EventLoop loop;
muduo::net::InetAddress addr(2007);
muduo::net::TcpServer server(&loop, addr, "Echo");
server.start();
loop.loop();
```

## Requirements

- CMake >= 3.16
- C++20 compiler
  - GCC >= 13
  - Clang >= 18
- Linux (primary target)
- Runtime dependency: Zlib (`libz`)

## Build and Install

Use `build.sh` (a wrapper over `CMakePresets.json`).

```bash
# Build and test.
./build.sh all release

# Build only.
./build.sh build release

# Test only.
./build.sh test release

# Install to a custom prefix.
./build.sh install release /opt/muduo-cpp-20
```

## Compatibility

- Existing muduo projects: core networking model and common APIs are preserved.
- New projects: C++20-style APIs are recommended (`string_view`, `span`, and `chrono`).
- Core library dependencies no longer require Boost.

### Installed package usage

```cmake
find_package(muduo CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE muduo::muduo_net)
```

### GitHub FetchContent usage

```cmake
include(FetchContent)
FetchContent_Declare(
  muduo_cpp20
  GIT_REPOSITORY https://github.com/CHAK-MING/muduo-cpp-20.git
  GIT_TAG 0.1.0
)
FetchContent_MakeAvailable(muduo_cpp20)

target_link_libraries(my_app PRIVATE muduo_net)
```

**Note:**
- `find_package` exports namespaced targets (`muduo::muduo_net`).
- `FetchContent` / `add_subdirectory` uses in-tree targets (`muduo_net`).

## Benchmark

Benchmark results as of 2026-02-11 (release build, CPU affinity set to core 2 with `taskset -c 2`).

**Environment:**
- OS: Ubuntu 25.10
- CPU: Intel Xeon Gold 5218 @ 2.30GHz (32 logical CPUs)
- Compiler: GCC 15.2.0

### Base Layer

| Benchmark | Case | Result |
|-----------|------|-------:|
| `logging_bench` | `BM_Logging_Stream` (1 thread, mean/median) | `101 ns/op` (~`9.90M msg/s`) |
| Original muduo `logging_test` | `nop` | `5,033,092.58 msg/s`, `537.06 MiB/s` |
| Relative gain | Logging throughput | **~`+96.7%`** |
| `threadpool_bench` | `BM_ThreadPool_Run/8/0/200000` (mean) | `437.978k tasks/s` |
| `threadpool_bench` | `BM_ThreadPool_Run/8/1024/200000` (mean) | `394.007k tasks/s` |

### Network Layer

| Payload | Mean latency | Throughput | QPS |
|--------:|-------------:|-----------:|----:|
| 64 B | `29.0 us` | `8.209 MiB/s` | `67.251k/s` |
| 256 B | `41.0 us` | `21.924 MiB/s` | `44.900k/s` |
| 1024 B | `40.6 us` | `86.210 MiB/s` | `44.140k/s` |
| 4096 B | `37.3 us` | `388.952 MiB/s` | `49.786k/s` |

Network throughput remains in the same order of magnitude as original muduo.

**Reproduce:**

```bash
./build/release/muduo/net/tests/net_echo_bench
./build/release/muduo/base/tests/threadpool_bench
./build/release/muduo/base/tests/logging_bench
```

## Roadmap

- [ ] Rewrite and validate `examples/` for the 0.1.x to 1.0.0 transition.
- [ ] Optimize hot paths while maintaining API compatibility.

## License

BSD 3-Clause. See [LICENSE](LICENSE).

[muduo]: https://github.com/chenshuo/muduo
