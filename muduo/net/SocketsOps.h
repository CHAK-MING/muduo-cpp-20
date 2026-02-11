#pragma once

#include <arpa/inet.h>
#include <cstddef>
#include <span>
#include <string_view>
#include <sys/types.h>
#include <sys/uio.h>

namespace muduo::net::sockets {

[[nodiscard]] int createNonblockingOrDie(sa_family_t family);

[[nodiscard]] int connect(int sockfd, const sockaddr *addr);
void bindOrDie(int sockfd, const sockaddr *addr);
void listenOrDie(int sockfd);
[[nodiscard]] int accept(int sockfd, sockaddr_in6 *addr);
#if MUDUO_ENABLE_LEGACY_COMPAT
[[nodiscard]] ssize_t read(int sockfd, void *buf, size_t count);
[[nodiscard]] ssize_t readv(int sockfd, const iovec *iov, int iovcnt);
[[nodiscard]] ssize_t write(int sockfd, const void *buf, size_t count);
#endif
[[nodiscard]] ssize_t read(int sockfd, std::span<std::byte> buffer);
[[nodiscard]] ssize_t read(int sockfd, std::span<char> buffer);
[[nodiscard]] ssize_t readv(int sockfd, std::span<const iovec> iov);
[[nodiscard]] ssize_t write(int sockfd, std::span<const std::byte> buffer);
[[nodiscard]] ssize_t write(int sockfd, std::span<const char> buffer);
void close(int sockfd);
void shutdownWrite(int sockfd);

#if MUDUO_ENABLE_LEGACY_COMPAT
void toIpPort(char *buf, size_t size, const sockaddr *addr);
void toIp(char *buf, size_t size, const sockaddr *addr);
#endif
[[nodiscard]] size_t toIpPortLen(char *buf, size_t size, const sockaddr *addr);
[[nodiscard]] size_t toIpLen(char *buf, size_t size, const sockaddr *addr);

void fromIpPort(std::string_view ip, uint16_t port, sockaddr_in *addr);
void fromIpPort(std::string_view ip, uint16_t port, sockaddr_in6 *addr);
#if MUDUO_ENABLE_LEGACY_COMPAT
void fromIpPort(const char *ip, uint16_t port, sockaddr_in *addr);
void fromIpPort(const char *ip, uint16_t port, sockaddr_in6 *addr);
#endif

[[nodiscard]] int getSocketError(int sockfd);

[[nodiscard]] const sockaddr *sockaddr_cast(const sockaddr_in *addr) noexcept;
[[nodiscard]] const sockaddr *sockaddr_cast(const sockaddr_in6 *addr) noexcept;
sockaddr *sockaddr_cast(sockaddr_in6 *addr) noexcept;
[[nodiscard]] const sockaddr_in *sockaddr_in_cast(const sockaddr *addr) noexcept;
[[nodiscard]] const sockaddr_in6 *sockaddr_in6_cast(const sockaddr *addr) noexcept;

[[nodiscard]] sockaddr_in6 getLocalAddr(int sockfd);
[[nodiscard]] sockaddr_in6 getPeerAddr(int sockfd);
[[nodiscard]] bool isSelfConnect(int sockfd);

} // namespace muduo::net::sockets
