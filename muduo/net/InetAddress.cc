#include "muduo/net/InetAddress.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Endian.h"
#include "muduo/net/SocketsOps.h"

#include <array>
#include <cassert>
#include <cstring>
#include <netdb.h>
#include <string_view>

using namespace muduo;
using namespace muduo::net;

namespace {

constexpr in_addr_t kInaddrAny = INADDR_ANY;
constexpr in_addr_t kInaddrLoopback = INADDR_LOOPBACK;

} // namespace

InetAddress::InetAddress(uint16_t portArg, bool loopbackOnly, bool ipv6) {
  storage_ = {};

  if (ipv6) {
    sockaddr_in6 addr6{};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_addr = loopbackOnly ? in6addr_loopback : in6addr_any;
    addr6.sin6_port = sockets::hostToNetwork16(portArg);
    setSockAddrIn6Internal(addr6);
    return;
  }

  sockaddr_in addr4{};
  addr4.sin_family = AF_INET;
  addr4.sin_addr.s_addr =
      sockets::hostToNetwork32(loopbackOnly ? kInaddrLoopback : kInaddrAny);
  addr4.sin_port = sockets::hostToNetwork16(portArg);
  setSockAddrIn(addr4);
}

InetAddress::InetAddress(std::string_view ip, uint16_t portArg, bool ipv6)
    : storage_{} {
  const bool treatAsIpv6 = ipv6 || ip.find(':') != std::string_view::npos;
  if (treatAsIpv6) {
    sockaddr_in6 addr6{};
    sockets::fromIpPort(ip, portArg, &addr6);
    setSockAddrIn6Internal(addr6);
    return;
  }

  sockaddr_in addr4{};
  sockets::fromIpPort(ip, portArg, &addr4);
  setSockAddrIn(addr4);
}

#if MUDUO_ENABLE_LEGACY_COMPAT
InetAddress::InetAddress(StringArg ip, uint16_t portArg, bool ipv6)
    : InetAddress(ip.as_string_view(), portArg, ipv6) {}

InetAddress::InetAddress(const char *ip, uint16_t portArg, bool ipv6)
    : InetAddress(std::string_view{ip == nullptr ? "" : ip}, portArg, ipv6) {}

InetAddress::InetAddress(const string &ip, uint16_t portArg, bool ipv6)
    : InetAddress(std::string_view{ip}, portArg, ipv6) {}
#endif

InetAddress::InetAddress(const sockaddr_in &addr) {
  storage_ = {};
  setSockAddrIn(addr);
}

InetAddress::InetAddress(const sockaddr_in6 &addr) {
  storage_ = {};
  setSockAddrIn6Internal(addr);
}

string InetAddress::toIpPort() const {
  std::array<char, 64> buf{};
  const auto len = sockets::toIpPortLen(buf.data(), buf.size(), getSockAddr());
  return std::string{buf.data(), len};
}

string InetAddress::toIp() const {
  std::array<char, 64> buf{};
  const auto len = sockets::toIpLen(buf.data(), buf.size(), getSockAddr());
  return std::string{buf.data(), len};
}

uint32_t InetAddress::ipv4NetEndian() const {
  assert(isIpv4());
  return asSockAddrIn().sin_addr.s_addr;
}

uint16_t InetAddress::port() const {
  return sockets::networkToHost16(portNetEndian());
}

void InetAddress::setSockAddrInet6(const sockaddr_in6 &addr6) {
  storage_ = {};
  setSockAddrIn6Internal(addr6);
}

uint16_t InetAddress::portNetEndian() const {
  if (isIpv6()) {
    return asSockAddrIn6().sin6_port;
  }
  if (isIpv4()) {
    return asSockAddrIn().sin_port;
  }
  return 0;
}

#if MUDUO_ENABLE_LEGACY_COMPAT
bool InetAddress::resolve(StringArg hostname, InetAddress *result) {
  return resolve(hostname.as_string_view(), result);
}

bool InetAddress::resolve(const char *hostname, InetAddress *result) {
  return resolve(std::string_view{hostname == nullptr ? "" : hostname}, result);
}
#endif

bool InetAddress::resolve(std::string_view hostname, InetAddress *result) {
  assert(result != nullptr);

  auto family = result->family();
  if (family != AF_INET && family != AF_INET6) {
    family = AF_UNSPEC;
  }
  return resolve(hostname, result, family);
}

#if MUDUO_ENABLE_LEGACY_COMPAT
bool InetAddress::resolve(StringArg hostname, InetAddress *result,
                          sa_family_t family) {
  return resolve(hostname.as_string_view(), result, family);
}
#endif

bool InetAddress::resolve(std::string_view hostname, InetAddress *result,
                          sa_family_t family) {
  assert(result != nullptr);
  const std::string hostnameStr(hostname);

  addrinfo hints{};
  hints.ai_family = family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG;

  addrinfo *resultList = nullptr;
  const int ret = ::getaddrinfo(hostnameStr.c_str(), nullptr, &hints, &resultList);
  if (ret != 0) {
    muduo::logError("InetAddress::resolve getaddrinfo failed for {}: {}",
                    hostnameStr, gai_strerror(ret));
    return false;
  }

  bool resolved = false;
  for (auto *ai = resultList; ai != nullptr; ai = ai->ai_next) {
    if (ai->ai_family == AF_INET &&
        ai->ai_addrlen >= static_cast<socklen_t>(sizeof(sockaddr_in))) {
      const auto *addr4 = sockets::sockaddr_in_cast(ai->ai_addr);
      const auto savedPort = result->portNetEndian();
      result->storage_ = {};
      sockaddr_in dst{};
      dst.sin_family = AF_INET;
      dst.sin_port = savedPort;
      result->setSockAddrIn(dst);
      result->setAddress(addr4->sin_addr);
      resolved = true;
      break;
    }

    if (ai->ai_family == AF_INET6 &&
        ai->ai_addrlen >= static_cast<socklen_t>(sizeof(sockaddr_in6))) {
      const auto *addr6 = sockets::sockaddr_in6_cast(ai->ai_addr);
      const auto savedPort = result->portNetEndian();
      result->storage_ = {};
      sockaddr_in6 dst{};
      dst.sin6_family = AF_INET6;
      dst.sin6_port = savedPort;
      result->setSockAddrIn6Internal(dst);
      result->setAddress(addr6->sin6_addr);
      resolved = true;
      break;
    }
  }

  ::freeaddrinfo(resultList);
  if (!resolved) {
    muduo::logError("InetAddress::resolve no usable address for {}",
                    hostnameStr);
  }
  return resolved;
}

void InetAddress::setScopeId(uint32_t scope_id) {
  if (isIpv6()) {
    auto addr6 = asSockAddrIn6();
    addr6.sin6_scope_id = scope_id;
    setSockAddrIn6Internal(addr6);
  }
}

sockaddr_in InetAddress::asSockAddrIn() const {
  sockaddr_in addr{};
  std::memcpy(&addr, &storage_, sizeof(addr));
  return addr;
}

sockaddr_in6 InetAddress::asSockAddrIn6() const {
  sockaddr_in6 addr6{};
  std::memcpy(&addr6, &storage_, sizeof(addr6));
  return addr6;
}

void InetAddress::setSockAddrIn(const sockaddr_in &addr) {
  storage_ = {};
  std::memcpy(&storage_, &addr, sizeof(addr));
}

void InetAddress::setSockAddrIn6Internal(const sockaddr_in6 &addr6) {
  storage_ = {};
  std::memcpy(&storage_, &addr6, sizeof(addr6));
}

void InetAddress::setAddress(const in_addr &addr) {
  assert(isIpv4());
  auto addr4 = asSockAddrIn();
  addr4.sin_addr = addr;
  setSockAddrIn(addr4);
}

void InetAddress::setAddress(const in6_addr &addr6) {
  assert(isIpv6());
  auto dst = asSockAddrIn6();
  dst.sin6_addr = addr6;
  setSockAddrIn6Internal(dst);
}
