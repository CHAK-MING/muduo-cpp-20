#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-all}"
PRESET="${2:-release}"
PREFIX="${3:-}"
JOBS="${JOBS:-$(nproc)}"

run_configure() {
  cmake --preset "${PRESET}"
}

run_build() {
  cmake --build --preset "${PRESET}" -j"${JOBS}"
}

run_test() {
  ctest --preset "${PRESET}"
}

run_install() {
  local build_dir="build/${PRESET}"
  if [[ -n "${PREFIX}" ]]; then
    cmake --install "${build_dir}" --prefix "${PREFIX}"
  else
    cmake --install "${build_dir}"
  fi
}

case "${MODE}" in
  configure)
    run_configure
    ;;
  build)
    run_build
    ;;
  test)
    run_test
    ;;
  install)
    run_install
    ;;
  all)
    run_configure
    run_build
    run_test
    ;;
  *)
    echo "Usage: $0 [all|configure|build|test|install] [preset] [install_prefix]"
    echo "Examples:"
    echo "  $0 all release"
    echo "  $0 install release /opt/muduo-cpp-20"
    exit 1
    ;;
esac
