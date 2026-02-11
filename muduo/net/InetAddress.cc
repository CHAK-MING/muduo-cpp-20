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

InetAddress::InetAddress(StringArg ip, uint16_t portArg, bool ipv6) {
  storage_ = {};

  const auto ipView = ip.as_string_view();
  const bool treatAsIpv6 = ipv6 || ipView.find(':') != std::string_view::npos;
  if (treatAsIpv6) {
    sockaddr_in6 addr6{};
    sockets::fromIpPort(ip.c_str(), portArg, &addr6);
    setSockAddrIn6Internal(addr6);
    return;
  }

  sockaddr_in addr4{};
  sockets::fromIpPort(ip.c_str(), portArg, &addr4);
  setSockAddrIn(addr4);
}

InetAddress::InetAddress(const char *ip, uint16_t portArg, bool ipv6)
    : InetAddress(StringArg(ip), portArg, ipv6) {}

InetAddress::InetAddress(std::string_view ip, uint16_t portArg, bool ipv6)
    : InetAddress(StringArg(ip), portArg, ipv6) {}

InetAddress::InetAddress(const string &ip, uint16_t portArg, bool ipv6)
    : InetAddress(StringArg(ip), portArg, ipv6) {}

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

bool InetAddress::resolve(StringArg hostname, InetAddress *out) {
  assert(out != nullptr);

  auto family = out->family();
  if (family != AF_INET && family != AF_INET6) {
    family = AF_UNSPEC;
  }
  return resolve(hostname, out, family);
}

bool InetAddress::resolve(const char *hostname, InetAddress *out) {
  return resolve(StringArg(hostname), out);
}

bool InetAddress::resolve(std::string_view hostname, InetAddress *out) {
  return resolve(StringArg(hostname), out);
}

bool InetAddress::resolve(StringArg hostname, InetAddress *out,
                          sa_family_t family) {
  assert(out != nullptr);

  addrinfo hints{};
  hints.ai_family = family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG;

  addrinfo *result = nullptr;
  const int ret = ::getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
  if (ret != 0) {
    LOG_ERROR << "InetAddress::resolve getaddrinfo failed for "
              << hostname.c_str() << ": " << gai_strerror(ret);
    return false;
  }

  bool resolved = false;
  for (auto *ai = result; ai != nullptr; ai = ai->ai_next) {
    if (ai->ai_family == AF_INET &&
        ai->ai_addrlen >= static_cast<socklen_t>(sizeof(sockaddr_in))) {
      const auto *addr4 = sockets::sockaddr_in_cast(ai->ai_addr);
      const auto savedPort = out->portNetEndian();
      out->storage_ = {};
      sockaddr_in dst{};
      dst.sin_family = AF_INET;
      dst.sin_port = savedPort;
      out->setSockAddrIn(dst);
      out->setAddress(addr4->sin_addr);
      resolved = true;
      break;
    }

    if (ai->ai_family == AF_INET6 &&
        ai->ai_addrlen >= static_cast<socklen_t>(sizeof(sockaddr_in6))) {
      const auto *addr6 = sockets::sockaddr_in6_cast(ai->ai_addr);
      const auto savedPort = out->portNetEndian();
      out->storage_ = {};
      sockaddr_in6 dst{};
      dst.sin6_family = AF_INET6;
      dst.sin6_port = savedPort;
      out->setSockAddrIn6Internal(dst);
      out->setAddress(addr6->sin6_addr);
      resolved = true;
      break;
    }
  }

  ::freeaddrinfo(result);
  if (!resolved) {
    LOG_ERROR << "InetAddress::resolve no usable address for "
              << hostname.c_str();
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
