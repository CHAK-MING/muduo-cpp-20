# Changelog

All notable changes to this project are documented in this file.

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
