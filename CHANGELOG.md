# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]

### Changed
- Continued C++20 modernization across `muduo/base` and `muduo/net`.
- Unified compatibility strategy around `MUDUO_ENABLE_LEGACY_COMPAT` for legacy surface control.
- Extended benchmark methodology:
  - Base layer refreshed with current logging and thread-pool benchmark numbers.
  - Network layer switched to EVPP-style ping-pong throughput matrix (connections x payload).

### Notes
- Ongoing 0.2.0 development includes broader internal cleanup, API tightening, and test/benchmark alignment.

## [0.1.0] - 2026-02-11

### Added
- CMake package export for `find_package(muduo CONFIG REQUIRED)` usage.
- `CMakePresets.json` for standardized configure/build/test flows.
- Additional benchmark coverage and benchmark reporting in README.
- `build.sh` preset-driven commands, including install support.

### Changed
- Refactored `muduo/base` internals to modern C++20 style.
- Refactored `muduo/net` internals to modern C++20 style while preserving Reactor semantics.
- Improved compatibility surface for legacy muduo usage patterns.
- Removed core Boost dependency from `muduo/base` and `muduo/net`.

### Tests
- Migrated and aligned test suites with GoogleTest across base and net modules.
- Expanded coverage for network paths and concurrency behavior.
- Repeated full-suite validation and benchmark reruns before tagging `0.1.0`.
