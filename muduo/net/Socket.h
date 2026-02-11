#pragma once

#include "muduo/base/noncopyable.h"

#include <source_location>
#include <sys/socket.h>

struct tcp_info;

namespace muduo::net {

class InetAddress;

class Socket : noncopyable {
public:
  explicit Socket(int sockfd) : sockfd_(sockfd) {}
  ~Socket();

  [[nodiscard]] int fd() const { return sockfd_; }

  [[nodiscard]] bool getTcpInfo(struct tcp_info *) const;
  [[nodiscard]] bool getTcpInfoString(char *buf, int len) const;

  void bindAddress(const InetAddress &localaddr) const;
  void listen() const;
  [[nodiscard]] int accept(InetAddress *peeraddr) const;

  void shutdownWrite() const;

  void setTcpNoDelay(bool on) const;
  void setReuseAddr(bool on) const;
  void setReusePort(bool on) const;
  void setKeepAlive(bool on) const;

private:
  [[nodiscard]] bool setSockOptOrLog(
      int level, int option, const void *optval, socklen_t optlen,
      const char *optionName,
      std::source_location loc = std::source_location::current()) const;

  const int sockfd_;
};

} // namespace muduo::net
