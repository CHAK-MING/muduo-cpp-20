# muduo-cpp-20

**Version:** 0.2.0

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
- Legacy compatibility APIs are opt-in.
  Define `MUDUO_ENABLE_LEGACY_COMPAT=1` to enable legacy interfaces and macros.

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

Method: `taskset -c 2`, Google Benchmark `real_time`.

Logging:

| Benchmark | Case | ns/op | msg/s | MiB/s |
|-----------|------|------:|------:|------:|
| `logging_bench` | `BM_Logging_Stream` (1 thread, 8-run mean/median) | `106 / 106` | `9.439M / 9.439M` | `1013.46 / 1013.41` |
| `logging_bench` | `BM_Logging_FormatConst` (1 thread, 8-run mean/median) | `94.3 / 94.3` | `10.607M / 10.607M` | `556.38 / 556.36` |
| Original muduo `logging_test` | `nop` (5 runs, mean/median) | - | `5.022M / 5.080M` | `526.27 / 532.41` |
| Relative gain | Stream throughput vs original mean | - | **~`+88.0%`** | **~`+92.6%`** |

### Network Layer

EVPP-style ping-pong throughput matrix (single client thread, 6s per case, Reactor echo loop):

| Connections | Payload | Throughput | QPS |
|------------:|--------:|-----------:|----:|
| 1 | 1024 B | `130.748 MiB/s` | `66.943k/s` |
| 1 | 4096 B | `494.548 MiB/s` | `63.302k/s` |
| 1 | 16384 B | `1759.613 MiB/s` | `56.308k/s` |
| 10 | 1024 B | `129.467 MiB/s` | `66.287k/s` |
| 10 | 4096 B | `487.468 MiB/s` | `62.396k/s` |
| 10 | 16384 B | `1747.535 MiB/s` | `55.921k/s` |
| 100 | 1024 B | `123.394 MiB/s` | `63.178k/s` |
| 100 | 4096 B | `457.712 MiB/s` | `58.587k/s` |
| 100 | 16384 B | `1547.394 MiB/s` | `49.517k/s` |

In the same harness, network throughput remains very close to original muduo (same order of magnitude).

## Roadmap

- [ ] Rewrite and validate `examples/` for the 0.1.x to 1.0.0 transition.
- [ ] Optimize hot paths while maintaining API compatibility.

## License

BSD 3-Clause. See [LICENSE](LICENSE).

[muduo]: https://github.com/chenshuo/muduo
