#include "muduo/net/Socket.h"

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <algorithm>
#include <cstdio>
#include <format>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

Socket::~Socket() { sockets::close(sockfd_); }

bool Socket::getTcpInfo(struct tcp_info *tcpi) const {
  auto len = static_cast<socklen_t>(sizeof(*tcpi));
  *tcpi = {};
  return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::getTcpInfoString(char *buf, int len) const {
  if (len <= 0) {
    return false;
  }
  tcp_info tcpi{};
  const bool ok = getTcpInfo(&tcpi);
  if (ok) {
    auto out = std::format_to_n(
        buf, static_cast<size_t>(len - 1),
        "unrecovered={} "
        "rto={} ato={} snd_mss={} rcv_mss={} "
        "lost={} retrans={} rtt={} rttvar={} "
        "sshthresh={} cwnd={} total_retrans={}",
        tcpi.tcpi_retransmits, tcpi.tcpi_rto, tcpi.tcpi_ato,
        tcpi.tcpi_snd_mss, tcpi.tcpi_rcv_mss, tcpi.tcpi_lost,
        tcpi.tcpi_retrans, tcpi.tcpi_rtt, tcpi.tcpi_rttvar,
        tcpi.tcpi_snd_ssthresh, tcpi.tcpi_snd_cwnd,
        tcpi.tcpi_total_retrans);
    const auto count = std::min<size_t>(out.size, static_cast<size_t>(len - 1));
    buf[count] = '\0';
  }
  return ok;
}

void Socket::bindAddress(const InetAddress &addr) const {
  sockets::bindOrDie(sockfd_, addr.getSockAddr());
}

void Socket::listen() const { sockets::listenOrDie(sockfd_); }

int Socket::accept(InetAddress *peeraddr) const {
  sockaddr_in6 addr{};
  int connfd = sockets::accept(sockfd_, &addr);
  if (connfd >= 0) {
    peeraddr->setSockAddrInet6(addr);
  }
  return connfd;
}

void Socket::shutdownWrite() const { sockets::shutdownWrite(sockfd_); }

void Socket::setTcpNoDelay(bool on) const {
  int optval = on ? 1 : 0;
  (void)setSockOptOrLog(IPPROTO_TCP, TCP_NODELAY, &optval,
                        static_cast<socklen_t>(sizeof optval), "TCP_NODELAY");
}

void Socket::setReuseAddr(bool on) const {
  int optval = on ? 1 : 0;
  (void)setSockOptOrLog(SOL_SOCKET, SO_REUSEADDR, &optval,
                        static_cast<socklen_t>(sizeof optval), "SO_REUSEADDR");
}

void Socket::setReusePort(bool on) const {
#ifdef SO_REUSEPORT
  int optval = on ? 1 : 0;
  const bool ok =
      setSockOptOrLog(SOL_SOCKET, SO_REUSEPORT, &optval,
                      static_cast<socklen_t>(sizeof optval), "SO_REUSEPORT");
  if (!ok && on) {
    muduo::logSysErr("SO_REUSEPORT failed");
  }
#else
  if (on) {
    muduo::logError("SO_REUSEPORT is not supported");
  }
#endif
}

void Socket::setKeepAlive(bool on) const {
  int optval = on ? 1 : 0;
  (void)setSockOptOrLog(SOL_SOCKET, SO_KEEPALIVE, &optval,
                        static_cast<socklen_t>(sizeof optval), "SO_KEEPALIVE");
}

bool Socket::setSockOptOrLog(int level, int option, const void *optval,
                             socklen_t optlen, const char *optionName,
                             std::source_location loc) const {
  if (::setsockopt(sockfd_, level, option, optval, optlen) == 0) {
    return true;
  }

  muduo::logSysErr("Socket::setSockOpt({}) failed at {}:{} ({})", optionName,
                   loc.file_name(), loc.line(), loc.function_name());
  return false;
}
