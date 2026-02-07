cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(muduo_20 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Boost is a required dependency for upcoming Asio-based modules.
# We require 1.85+ to keep behavior and API expectations consistent.
find_package(Boost 1.85 REQUIRED COMPONENTS system)
message(STATUS "Found Boost ${Boost_VERSION_STRING}")

# Compiler version requirements for full C++20 support
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.0")
    message(FATAL_ERROR "GCC version must be at least 13.0 for full C++20 support")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18.0")
    message(FATAL_ERROR "Clang version must be at least 18.0 for full C++20 support")
  endif()
endif()

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)

  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  # Note: Tests are now handled in muduo/base/tests/CMakeLists.txt
endif()

# Build muduo_base library
add_subdirectory(muduo/base)
