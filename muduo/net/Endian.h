#pragma once

#include "muduo/base/CxxFeatures.h"

#include <bit>
#include <cstdint>

namespace muduo::net::sockets {

static_assert(std::endian::native == std::endian::little ||
                  std::endian::native == std::endian::big,
              "unsupported mixed-endian architecture");

constexpr uint16_t byteSwap16(uint16_t value) noexcept {
#if MUDUO_HAS_CPP23_BYTESWAP
  return std::byteswap(value);
#else
  return static_cast<uint16_t>((value << 8) | (value >> 8));
#endif
}

constexpr uint32_t byteSwap32(uint32_t value) noexcept {
#if MUDUO_HAS_CPP23_BYTESWAP
  return std::byteswap(value);
#else
  return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) |
         ((value & 0x00FF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
#endif
}

constexpr uint64_t byteSwap64(uint64_t value) noexcept {
#if MUDUO_HAS_CPP23_BYTESWAP
  return std::byteswap(value);
#else
  return ((value & 0x00000000000000FFull) << 56) |
         ((value & 0x000000000000FF00ull) << 40) |
         ((value & 0x0000000000FF0000ull) << 24) |
         ((value & 0x00000000FF000000ull) << 8) |
         ((value & 0x000000FF00000000ull) >> 8) |
         ((value & 0x0000FF0000000000ull) >> 24) |
         ((value & 0x00FF000000000000ull) >> 40) |
         ((value & 0xFF00000000000000ull) >> 56);
#endif
}

constexpr uint64_t hostToNetwork64(uint64_t host64) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return byteSwap64(host64);
  }
  return host64;
}

constexpr uint32_t hostToNetwork32(uint32_t host32) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return byteSwap32(host32);
  }
  return host32;
}

constexpr uint16_t hostToNetwork16(uint16_t host16) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return byteSwap16(host16);
  }
  return host16;
}

constexpr uint64_t networkToHost64(uint64_t net64) noexcept {
  return hostToNetwork64(net64);
}

constexpr uint32_t networkToHost32(uint32_t net32) noexcept {
  return hostToNetwork32(net32);
}

constexpr uint16_t networkToHost16(uint16_t net16) noexcept {
  return hostToNetwork16(net16);
}

} // namespace muduo::net::sockets
