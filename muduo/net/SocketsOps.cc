#include "muduo/net/SocketsOps.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Types.h"
#include "muduo/net/Endian.h"

#include <algorithm>
#include <cerrno>
#include <array>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <limits>
#include <string>
#include <utility>
#include <sys/socket.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace {

template <typename... Args>
size_t formatToBuffer(char *buf, size_t size, std::format_string<Args...> fmt,
                      Args &&...args) {
  if (size == 0) {
    return 0;
  }
  auto out = std::format_to_n(buf, size - 1, fmt, std::forward<Args>(args)...);
  const size_t count = std::min<size_t>(out.size, size - 1);
  buf[count] = '\0';
  return count;
}

using SA = sockaddr;

#if VALGRIND || defined(NO_ACCEPT4)
void setNonBlockAndCloseOnExec(int sockfd) {
  int flags = ::fcntl(sockfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int ret = ::fcntl(sockfd, F_SETFL, flags);

  flags = ::fcntl(sockfd, F_GETFD, 0);
  flags |= FD_CLOEXEC;
  ret = ::fcntl(sockfd, F_SETFD, flags);

  (void)ret;
}
#endif

} // namespace

const sockaddr *sockets::sockaddr_cast(const sockaddr_in6 *addr) noexcept {
  return reinterpret_cast<const sockaddr *>(addr);
}

sockaddr *sockets::sockaddr_cast(sockaddr_in6 *addr) noexcept {
  return reinterpret_cast<sockaddr *>(addr);
}

const sockaddr *sockets::sockaddr_cast(const sockaddr_in *addr) noexcept {
  return reinterpret_cast<const sockaddr *>(addr);
}

const sockaddr_in *sockets::sockaddr_in_cast(const sockaddr *addr) noexcept {
  return reinterpret_cast<const sockaddr_in *>(addr);
}

const sockaddr_in6 *sockets::sockaddr_in6_cast(const sockaddr *addr) noexcept {
  return reinterpret_cast<const sockaddr_in6 *>(addr);
}

int sockets::createNonblockingOrDie(sa_family_t family) {
#if VALGRIND
  int sockfd = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0) {
    LOG_SYSFATAL << "sockets::createNonblockingOrDie";
  }
  setNonBlockAndCloseOnExec(sockfd);
#else
  int sockfd =
      ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
  if (sockfd < 0) {
    LOG_SYSFATAL << "sockets::createNonblockingOrDie";
  }
#endif
  return sockfd;
}

void sockets::bindOrDie(int sockfd, const sockaddr *addr) {
  int ret = ::bind(sockfd, addr, static_cast<socklen_t>(sizeof(sockaddr_in6)));
  if (ret < 0) {
    LOG_SYSFATAL << "sockets::bindOrDie";
  }
}

void sockets::listenOrDie(int sockfd) {
  int ret = ::listen(sockfd, SOMAXCONN);
  if (ret < 0) {
    LOG_SYSFATAL << "sockets::listenOrDie";
  }
}

int sockets::accept(int sockfd, sockaddr_in6 *addr) {
  auto addrlen = static_cast<socklen_t>(sizeof *addr);
#if VALGRIND || defined(NO_ACCEPT4)
  int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen);
  setNonBlockAndCloseOnExec(connfd);
#else
  int connfd = ::accept4(sockfd, sockaddr_cast(addr), &addrlen,
                         SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
  if (connfd < 0) {
    int savedErrno = errno;
    LOG_SYSERR << "Socket::accept";
    switch (savedErrno) {
    case EAGAIN:
    case ECONNABORTED:
    case EINTR:
    case EPROTO:
    case EPERM:
    case EMFILE:
      errno = savedErrno;
      break;
    case EBADF:
    case EFAULT:
    case EINVAL:
    case ENFILE:
    case ENOBUFS:
    case ENOMEM:
    case ENOTSOCK:
    case EOPNOTSUPP:
      LOG_FATAL << "unexpected error of ::accept " << savedErrno;
      break;
    default:
      LOG_FATAL << "unknown error of ::accept " << savedErrno;
      break;
    }
  }
  return connfd;
}

int sockets::connect(int sockfd, const sockaddr *addr) {
  return ::connect(sockfd, addr, static_cast<socklen_t>(sizeof(sockaddr_in6)));
}

ssize_t sockets::read(int sockfd, void *buf, size_t count) {
  return ::read(sockfd, buf, count);
}

ssize_t sockets::read(int sockfd, std::span<std::byte> buffer) {
  return ::read(sockfd, buffer.data(), buffer.size_bytes());
}

ssize_t sockets::read(int sockfd, std::span<char> buffer) {
  return ::read(sockfd, buffer.data(), buffer.size_bytes());
}

ssize_t sockets::readv(int sockfd, const iovec *iov, int iovcnt) {
  return ::readv(sockfd, iov, iovcnt);
}

ssize_t sockets::readv(int sockfd, std::span<const iovec> iov) {
  if (iov.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    errno = EINVAL;
    return -1;
  }
  return ::readv(sockfd, iov.data(), static_cast<int>(iov.size()));
}

ssize_t sockets::write(int sockfd, const void *buf, size_t count) {
  return ::write(sockfd, buf, count);
}

ssize_t sockets::write(int sockfd, std::span<const std::byte> buffer) {
  return ::write(sockfd, buffer.data(), buffer.size_bytes());
}

ssize_t sockets::write(int sockfd, std::span<const char> buffer) {
  return ::write(sockfd, buffer.data(), buffer.size_bytes());
}

void sockets::close(int sockfd) {
  if (::close(sockfd) < 0) {
    LOG_SYSERR << "sockets::close";
  }
}

void sockets::shutdownWrite(int sockfd) {
  if (::shutdown(sockfd, SHUT_WR) < 0) {
    LOG_SYSERR << "sockets::shutdownWrite";
  }
}

size_t sockets::toIpPortLen(char *buf, size_t size, const sockaddr *addr) {
  if (size == 0) {
    return 0;
  }

  if (addr->sa_family == AF_INET6) {
    std::array<char, INET6_ADDRSTRLEN> ip{};
    const auto *addr6 = sockaddr_in6_cast(addr);
    if (::inet_ntop(AF_INET6, &addr6->sin6_addr, ip.data(),
                    static_cast<socklen_t>(ip.size())) == nullptr) {
      buf[0] = '\0';
      return 0;
    }
    const uint16_t port = sockets::networkToHost16(addr6->sin6_port);
    return formatToBuffer(buf, size, "[{}]:{}", ip.data(), port);
  }

  if (addr->sa_family == AF_INET) {
    std::array<char, INET_ADDRSTRLEN> ip{};
    const auto *addr4 = sockaddr_in_cast(addr);
    if (::inet_ntop(AF_INET, &addr4->sin_addr, ip.data(),
                    static_cast<socklen_t>(ip.size())) == nullptr) {
      buf[0] = '\0';
      return 0;
    }
    const uint16_t port = sockets::networkToHost16(addr4->sin_port);
    return formatToBuffer(buf, size, "{}:{}", ip.data(), port);
  }

  buf[0] = '\0';
  return 0;
}

size_t sockets::toIpLen(char *buf, size_t size, const sockaddr *addr) {
  if (size == 0) {
    return 0;
  }

  if (addr->sa_family == AF_INET) {
    assert(size >= INET_ADDRSTRLEN);
    const auto *addr4 = sockaddr_in_cast(addr);
    if (::inet_ntop(AF_INET, &addr4->sin_addr, buf,
                    static_cast<socklen_t>(size)) == nullptr) {
      buf[0] = '\0';
      return 0;
    }
    return std::char_traits<char>::length(buf);
  } else if (addr->sa_family == AF_INET6) {
    assert(size >= INET6_ADDRSTRLEN);
    const auto *addr6 = sockaddr_in6_cast(addr);
    if (::inet_ntop(AF_INET6, &addr6->sin6_addr, buf,
                    static_cast<socklen_t>(size)) == nullptr) {
      buf[0] = '\0';
      return 0;
    }
    return std::char_traits<char>::length(buf);
  }
  buf[0] = '\0';
  return 0;
}

void sockets::toIpPort(char *buf, size_t size, const sockaddr *addr) {
  (void)toIpPortLen(buf, size, addr);
}

void sockets::toIp(char *buf, size_t size, const sockaddr *addr) {
  (void)toIpLen(buf, size, addr);
}

void sockets::fromIpPort(const char *ip, uint16_t port, sockaddr_in *addr) {
  addr->sin_family = AF_INET;
  addr->sin_port = hostToNetwork16(port);
  if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
    LOG_SYSERR << "sockets::fromIpPort";
  }
}

void sockets::fromIpPort(const char *ip, uint16_t port, sockaddr_in6 *addr) {
  addr->sin6_family = AF_INET6;
  addr->sin6_port = hostToNetwork16(port);
  if (::inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0) {
    LOG_SYSERR << "sockets::fromIpPort";
  }
}

int sockets::getSocketError(int sockfd) {
  int optval;
  auto optlen = static_cast<socklen_t>(sizeof optval);

  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return errno;
  }
  return optval;
}

sockaddr_in6 sockets::getLocalAddr(int sockfd) {
  sockaddr_in6 localaddr{};
  auto addrlen = static_cast<socklen_t>(sizeof localaddr);
  if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0) {
    LOG_SYSERR << "sockets::getLocalAddr";
  }
  return localaddr;
}

sockaddr_in6 sockets::getPeerAddr(int sockfd) {
  sockaddr_in6 peeraddr{};
  auto addrlen = static_cast<socklen_t>(sizeof peeraddr);
  if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0) {
    LOG_SYSERR << "sockets::getPeerAddr";
  }
  return peeraddr;
}

bool sockets::isSelfConnect(int sockfd) {
  auto localaddr = getLocalAddr(sockfd);
  auto peeraddr = getPeerAddr(sockfd);
  if (localaddr.sin6_family == AF_INET) {
    sockaddr_in laddr4{};
    sockaddr_in raddr4{};
    std::memcpy(&laddr4, &localaddr, sizeof(laddr4));
    std::memcpy(&raddr4, &peeraddr, sizeof(raddr4));
    return laddr4.sin_port == raddr4.sin_port &&
           laddr4.sin_addr.s_addr == raddr4.sin_addr.s_addr;
  }
  if (localaddr.sin6_family == AF_INET6) {
    return localaddr.sin6_port == peeraddr.sin6_port &&
           IN6_ARE_ADDR_EQUAL(&localaddr.sin6_addr, &peeraddr.sin6_addr);
  }
  return false;
}
