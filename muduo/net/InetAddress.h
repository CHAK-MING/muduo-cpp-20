#pragma once

#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include "muduo/base/copyable.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <string_view>

namespace muduo::net {

class InetAddress : public muduo::copyable {
public:
  explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false,
                       bool ipv6 = false);

  InetAddress(StringArg ip, uint16_t port, bool ipv6 = false);
  InetAddress(const char *ip, uint16_t port, bool ipv6 = false);
  InetAddress(std::string_view ip, uint16_t port, bool ipv6 = false);
  InetAddress(const string &ip, uint16_t port, bool ipv6 = false);

  explicit InetAddress(const sockaddr_in &addr);
  explicit InetAddress(const sockaddr_in6 &addr);

  [[nodiscard]] sa_family_t family() const { return storage_.ss_family; }
  [[nodiscard]] bool isIpv4() const { return family() == AF_INET; }
  [[nodiscard]] bool isIpv6() const { return family() == AF_INET6; }

  [[nodiscard]] string toIp() const;
  [[nodiscard]] string toIpPort() const;
  [[nodiscard]] uint16_t port() const;

  [[nodiscard]] const sockaddr *getSockAddr() const {
    return reinterpret_cast<const sockaddr *>(&storage_);
  }
  void setSockAddrInet6(const sockaddr_in6 &addr6);

  [[nodiscard]] uint32_t ipv4NetEndian() const;
  [[nodiscard]] uint16_t portNetEndian() const;

  [[nodiscard]] static bool resolve(StringArg hostname, InetAddress *result);
  [[nodiscard]] static bool resolve(StringArg hostname, InetAddress *result,
                                    sa_family_t family);
  [[nodiscard]] static bool resolve(const char *hostname, InetAddress *result);
  [[nodiscard]] static bool resolve(std::string_view hostname,
                                    InetAddress *result);

  void setScopeId(uint32_t scope_id);

private:
  [[nodiscard]] sockaddr_in asSockAddrIn() const;
  [[nodiscard]] sockaddr_in6 asSockAddrIn6() const;
  void setSockAddrIn(const sockaddr_in &addr);
  void setSockAddrIn6Internal(const sockaddr_in6 &addr6);

  void setAddress(const in_addr &addr);
  void setAddress(const in6_addr &addr6);

  sockaddr_storage storage_{};
};

} // namespace muduo::net
