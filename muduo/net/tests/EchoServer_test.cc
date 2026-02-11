#include "muduo/net/TcpServer.h"

#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <tuple>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int pickPort(bool ipv6) {
  const int domain = ipv6 ? AF_INET6 : AF_INET;
  const int fd = ::socket(domain, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return 0;
  }

  int one = 1;
  (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  int port = 0;
  if (ipv6) {
    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_loopback;
    addr.sin6_port = 0;
    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
      sockaddr_in6 local{};
      socklen_t len = sizeof(local);
      if (::getsockname(fd, reinterpret_cast<sockaddr *>(&local), &len) == 0) {
        port = ntohs(local.sin6_port);
      }
    }
  } else {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
      sockaddr_in local{};
      socklen_t len = sizeof(local);
      if (::getsockname(fd, reinterpret_cast<sockaddr *>(&local), &len) == 0) {
        port = ntohs(local.sin_port);
      }
    }
  }
  ::close(fd);
  return port;
}

class EchoServer {
public:
  EchoServer(muduo::net::EventLoop *loop, const muduo::net::InetAddress &addr,
             int threadNum)
      : loop_(loop), server_(loop, addr, "EchoServerTest") {
    server_.setConnectionCallback(
        [this](const muduo::net::TcpConnectionPtr &conn) {
          if (conn->connected()) {
            conn->send(std::string_view{"hello\n"});
          }
        });
    server_.setMessageCallback(
        [this](const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf,
               muduo::Timestamp) {
          const auto msg = buf->retrieveAllAsString();
          if (msg == "exit\n") {
            conn->send(std::string_view{"bye\n"});
            conn->shutdown();
          }
          if (msg == "quit\n") {
            loop_->quit();
          }
          conn->send(std::string_view{msg});
        });
    server_.setThreadNum(threadNum);
  }

  void start() { server_.start(); }

private:
  muduo::net::EventLoop *loop_;
  muduo::net::TcpServer server_;
};

class EchoServerTest : public ::testing::Test {};
class EchoServerParamTest
    : public ::testing::TestWithParam<std::tuple<bool, int>> {};

ssize_t readSome(int fd, std::span<char> buffer) {
  return muduo::net::sockets::read(fd, buffer);
}

bool writeExact(int fd, std::string_view payload) {
  return muduo::net::sockets::write(fd, std::span<const char>{payload.data(), payload.size()}) ==
         static_cast<ssize_t>(payload.size());
}

bool runEchoClientV4(int port, std::atomic<bool> &gotHello) {
  const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
    ::close(fd);
    return false;
  }
  if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return false;
  }

  std::array<char, 128> buf{};
  const auto n = readSome(fd, buf);
  if (n > 0) {
    std::string s(buf.data(), static_cast<size_t>(n));
    if (s.find("hello\n") != std::string::npos) {
      gotHello.store(true, std::memory_order_release);
    }
  }

  const bool writeOk = writeExact(fd, "quit\n");
  ::close(fd);
  return writeOk;
}

bool runEchoClientV4Exit(int port, std::atomic<bool> &gotBye) {
  const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
    ::close(fd);
    return false;
  }
  if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return false;
  }

  std::array<char, 128> helloBuf{};
  (void)readSome(fd, helloBuf);

  if (!writeExact(fd, "exit\n")) {
    ::close(fd);
    return false;
  }

  std::array<char, 256> buf{};
  const auto n = readSome(fd, buf);
  if (n > 0) {
    std::string s(buf.data(), static_cast<size_t>(n));
    if (s.find("bye\n") != std::string::npos) {
      gotBye.store(true, std::memory_order_release);
    }
  }

  ::close(fd);
  return true;
}

bool runEchoClientV6(int port, std::atomic<bool> &gotHello) {
  const int fd = ::socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return false;
  }

  sockaddr_in6 addr{};
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(static_cast<uint16_t>(port));
  if (::inet_pton(AF_INET6, "::1", &addr.sin6_addr) != 1) {
    ::close(fd);
    return false;
  }
  if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return false;
  }

  std::array<char, 128> buf{};
  const auto n = readSome(fd, buf);
  if (n > 0) {
    std::string s(buf.data(), static_cast<size_t>(n));
    if (s.find("hello\n") != std::string::npos) {
      gotHello.store(true, std::memory_order_release);
    }
  }

  const bool writeOk = writeExact(fd, "quit\n");
  ::close(fd);
  return writeOk;
}

void runEchoServerCase(bool ipv6, int threadNum) {
  using namespace std::chrono_literals;

  const int port = pickPort(ipv6);
  ASSERT_GT(port, 0);
  muduo::net::EventLoop loop;
  muduo::net::InetAddress listenAddr(static_cast<uint16_t>(port), true, ipv6);
  EchoServer server(&loop, listenAddr, threadNum);
  server.start();

  std::atomic<bool> gotHello{false};
  std::atomic<bool> clientOk{true};
  std::thread client([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const bool ok = ipv6 ? runEchoClientV6(port, gotHello) : runEchoClientV4(port, gotHello);
    clientOk.store(ok, std::memory_order_release);
  });

  (void)loop.runAfter(2s, [&loop] { loop.quit(); });
  loop.loop();
  client.join();

  ASSERT_TRUE(clientOk.load(std::memory_order_acquire));
  EXPECT_TRUE(gotHello.load(std::memory_order_acquire));
}

} // namespace

TEST_P(EchoServerParamTest, EchoFlowVariants) {
  const auto [ipv6, threadNum] = GetParam();
  runEchoServerCase(ipv6, threadNum);
}

INSTANTIATE_TEST_SUITE_P(
    EchoCases, EchoServerParamTest,
    ::testing::Values(std::tuple{false, 0}, std::tuple{false, 2},
                      std::tuple{true, 0}));

TEST_F(EchoServerTest, ExitCommandSendsByeAndShutdown) {
  using namespace std::chrono_literals;

  const int port = pickPort(false);
  ASSERT_GT(port, 0);
  muduo::net::EventLoop loop;
  muduo::net::InetAddress listenAddr(static_cast<uint16_t>(port), true, false);
  EchoServer server(&loop, listenAddr, 0);
  server.start();

  std::atomic<bool> gotBye{false};
  std::atomic<bool> clientOk{true};
  std::thread client([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    clientOk.store(runEchoClientV4Exit(port, gotBye), std::memory_order_release);
  });

  (void)loop.runAfter(800ms, [&loop] { loop.quit(); });
  loop.loop();
  client.join();

  ASSERT_TRUE(clientOk.load(std::memory_order_acquire));
  EXPECT_TRUE(gotBye.load(std::memory_order_acquire));
}
