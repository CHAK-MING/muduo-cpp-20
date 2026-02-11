#include "muduo/net/TcpClient.h"

#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <concepts>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

using namespace std::chrono_literals;
using namespace std::string_view_literals;

class EchoClientTest : public ::testing::Test {
};
class EchoClientBasicParamTest : public ::testing::TestWithParam<bool> {};

class OriginalStyleEchoClient {
public:
  using OnConnectedCallback = muduo::net::CallbackFunction<void()>;
  using OnShutdownCallback = muduo::net::CallbackFunction<void()>;
  using OnQuitCallback = muduo::net::CallbackFunction<void()>;

  OriginalStyleEchoClient(muduo::net::EventLoop *loop,
                          const muduo::net::InetAddress &serverAddr, std::string id)
      : loop_(loop), client_(loop, serverAddr, "EchoClient" + std::move(id)) {
    client_.setConnectionCallback([this](const muduo::net::TcpConnectionPtr &conn) {
      if (conn->connected()) {
        if (onConnected_) {
          onConnected_();
        }
      }
      conn->send("world\n"sv);
    });
    client_.setMessageCallback([this](const muduo::net::TcpConnectionPtr &conn,
                                      muduo::net::Buffer *buf, muduo::Timestamp) {
      const auto msg = buf->retrieveAllAsString();
      if (msg == "quit\n") {
        if (onQuit_) {
          onQuit_();
        }
        conn->send("bye\n"sv);
        conn->shutdown();
      } else if (msg == "shutdown\n") {
        if (onShutdown_) {
          onShutdown_();
        }
        loop_->quit();
      } else {
        conn->send(std::string_view{msg});
      }
    });
  }

  void connect() { client_.connect(); }

  template <typename F>
    requires std::constructible_from<OnConnectedCallback, F>
  void setOnConnected(F &&cb) {
    onConnected_ = OnConnectedCallback(std::forward<F>(cb));
  }

  template <typename F>
    requires std::constructible_from<OnShutdownCallback, F>
  void setOnShutdown(F &&cb) {
    onShutdown_ = OnShutdownCallback(std::forward<F>(cb));
  }

  template <typename F>
    requires std::constructible_from<OnQuitCallback, F>
  void setOnQuit(F &&cb) {
    onQuit_ = OnQuitCallback(std::forward<F>(cb));
  }

private:
  muduo::net::EventLoop *loop_;
  muduo::net::TcpClient client_;
  OnConnectedCallback onConnected_;
  OnShutdownCallback onShutdown_;
  OnQuitCallback onQuit_;
};

ssize_t readSome(int fd, std::span<char> buffer) {
  return muduo::net::sockets::read(fd, buffer);
}

bool writeExact(int fd, std::string_view payload) {
  return muduo::net::sockets::write(fd, std::span<const char>{payload.data(), payload.size()}) ==
         static_cast<ssize_t>(payload.size());
}

int startScriptedServerForShutdown(uint16_t &portOut, std::atomic<bool> &ready,
                                   std::atomic<bool> &gotWorld) {
  const int listenfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listenfd < 0) {
    return -1;
  }
  int one = 1;
  ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (::bind(listenfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    ::close(listenfd);
    return -1;
  }
  if (::listen(listenfd, SOMAXCONN) != 0) {
    ::close(listenfd);
    return -1;
  }
  sockaddr_in local{};
  socklen_t len = sizeof(local);
  if (::getsockname(listenfd, reinterpret_cast<sockaddr *>(&local), &len) != 0) {
    ::close(listenfd);
    return -1;
  }
  portOut = ntohs(local.sin_port);
  ready.store(true, std::memory_order_release);

  const int connfd = ::accept4(listenfd, nullptr, nullptr, SOCK_CLOEXEC);
  if (connfd >= 0) {
    std::array<char, 256> buf{};
    const ssize_t n = readSome(connfd, buf);
    if (n > 0) {
      const std::string msg(buf.data(), static_cast<size_t>(n));
      if (msg.find("world\n") != std::string::npos) {
        gotWorld.store(true, std::memory_order_release);
      }
      (void)writeExact(connfd, "shutdown\n");
    }
    ::close(connfd);
  }
  ::close(listenfd);
  return 0;
}

int startScriptedServerForShutdownV6(uint16_t &portOut, std::atomic<bool> &ready,
                                     std::atomic<bool> &gotWorld) {
  const int listenfd = ::socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listenfd < 0) {
    return -1;
  }
  int one = 1;
  ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in6 addr{};
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_loopback;
  addr.sin6_port = 0;
  if (::bind(listenfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    ::close(listenfd);
    return -1;
  }
  if (::listen(listenfd, SOMAXCONN) != 0) {
    ::close(listenfd);
    return -1;
  }
  sockaddr_in6 local{};
  socklen_t len = sizeof(local);
  if (::getsockname(listenfd, reinterpret_cast<sockaddr *>(&local), &len) != 0) {
    ::close(listenfd);
    return -1;
  }
  portOut = ntohs(local.sin6_port);
  ready.store(true, std::memory_order_release);

  const int connfd = ::accept4(listenfd, nullptr, nullptr, SOCK_CLOEXEC);
  if (connfd >= 0) {
    std::array<char, 256> buf{};
    const ssize_t n = readSome(connfd, buf);
    if (n > 0) {
      const std::string msg(buf.data(), static_cast<size_t>(n));
      if (msg.find("world\n") != std::string::npos) {
        gotWorld.store(true, std::memory_order_release);
      }
      (void)writeExact(connfd, "shutdown\n");
    }
    ::close(connfd);
  }
  ::close(listenfd);
  return 0;
}

int startScriptedServerForQuit(uint16_t &portOut, std::atomic<bool> &ready,
                               std::atomic<bool> &gotBye) {
  const int listenfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listenfd < 0) {
    return -1;
  }
  int one = 1;
  ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (::bind(listenfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    ::close(listenfd);
    return -1;
  }
  if (::listen(listenfd, SOMAXCONN) != 0) {
    ::close(listenfd);
    return -1;
  }
  sockaddr_in local{};
  socklen_t len = sizeof(local);
  if (::getsockname(listenfd, reinterpret_cast<sockaddr *>(&local), &len) != 0) {
    ::close(listenfd);
    return -1;
  }
  portOut = ntohs(local.sin_port);
  ready.store(true, std::memory_order_release);

  const int connfd = ::accept4(listenfd, nullptr, nullptr, SOCK_CLOEXEC);
  if (connfd >= 0) {
    std::array<char, 256> buf{};
    (void)readSome(connfd, buf); // read "world\n"
    (void)writeExact(connfd, "quit\n");

    const ssize_t n = readSome(connfd, buf); // expect "bye\n"
    if (n > 0) {
      const std::string msg(buf.data(), static_cast<size_t>(n));
      if (msg.find("bye\n") != std::string::npos) {
        gotBye.store(true, std::memory_order_release);
      }
    }
    (void)writeExact(connfd, "shutdown\n");
    ::close(connfd);
  }
  ::close(listenfd);
  return 0;
}

int startScriptedServerForMultiClient(uint16_t &portOut, std::atomic<bool> &ready,
                                      std::atomic<int> &gotWorldCount, int expectedClients) {
  const int listenfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listenfd < 0) {
    return -1;
  }
  int one = 1;
  ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (::bind(listenfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    ::close(listenfd);
    return -1;
  }
  if (::listen(listenfd, SOMAXCONN) != 0) {
    ::close(listenfd);
    return -1;
  }
  sockaddr_in local{};
  socklen_t len = sizeof(local);
  if (::getsockname(listenfd, reinterpret_cast<sockaddr *>(&local), &len) != 0) {
    ::close(listenfd);
    return -1;
  }
  portOut = ntohs(local.sin_port);
  ready.store(true, std::memory_order_release);

  for (int i = 0; i < expectedClients; ++i) {
    const int connfd = ::accept4(listenfd, nullptr, nullptr, SOCK_CLOEXEC);
    if (connfd < 0) {
      continue;
    }
    std::array<char, 256> buf{};
    const ssize_t n = readSome(connfd, buf);
    if (n > 0) {
      const std::string msg(buf.data(), static_cast<size_t>(n));
      if (msg.find("world\n") != std::string::npos) {
        gotWorldCount.fetch_add(1, std::memory_order_relaxed);
      }
    }
    ::close(connfd);
  }
  ::close(listenfd);
  return 0;
}

bool waitServerReady(const std::atomic<bool> &ready) {
  using namespace std::chrono_literals;
  for (int i = 0; i < 400; ++i) {
    if (ready.load(std::memory_order_acquire)) {
      return true;
    }
    std::this_thread::sleep_for(5ms);
  }
  return false;
}

} // namespace

TEST_P(EchoClientBasicParamTest, BasicEchoFlow) {
  const bool ipv6 = GetParam();
  std::atomic<bool> serverReady{false};
  std::atomic<bool> gotWorld{false};
  std::atomic<bool> shutdownHandled{false};
  uint16_t port = 0;

  std::thread serverThread([&] {
    if (ipv6) {
      (void)startScriptedServerForShutdownV6(port, serverReady, gotWorld);
    } else {
      (void)startScriptedServerForShutdown(port, serverReady, gotWorld);
    }
  });

  const bool readyInTime = waitServerReady(serverReady);
  if (!readyInTime) {
    serverThread.join();
    FAIL() << "Raw echo server failed to start in time (ipv6=" << ipv6 << ")";
  }

  muduo::net::EventLoop loop;
  OriginalStyleEchoClient client(&loop,
                                 ipv6 ? muduo::net::InetAddress("::1"sv, port, true)
                                      : muduo::net::InetAddress("127.0.0.1"sv, port),
                                 ipv6 ? "BasicV6" : "Basic");
  client.setOnShutdown([&] { shutdownHandled.store(true, std::memory_order_release); });
  client.connect();
  (void)loop.runAfter(2s, [&loop] { loop.quit(); });
  loop.loop();

  serverThread.join();
  EXPECT_TRUE(gotWorld.load(std::memory_order_acquire));
  EXPECT_TRUE(shutdownHandled.load(std::memory_order_acquire));
}

INSTANTIATE_TEST_SUITE_P(TransportFamily, EchoClientBasicParamTest,
                         ::testing::Values(false, true));

TEST_F(EchoClientTest, QuitCommandSendsByeAndThenShutdownQuitsLoop) {
  std::atomic<bool> serverReady{false};
  std::atomic<bool> gotBye{false};
  std::atomic<bool> quitHandled{false};
  std::atomic<bool> shutdownHandled{false};
  uint16_t port = 0;

  std::thread serverThread([&] {
    (void)startScriptedServerForQuit(port, serverReady, gotBye);
  });

  const bool readyInTime = waitServerReady(serverReady);
  if (!readyInTime) {
    serverThread.join();
    FAIL() << "Raw quit server failed to start in time";
  }

  muduo::net::EventLoop loop;
  OriginalStyleEchoClient client(
      &loop, muduo::net::InetAddress("127.0.0.1"sv, port), "Quit");
  client.setOnQuit([&] { quitHandled.store(true, std::memory_order_release); });
  client.setOnShutdown([&] { shutdownHandled.store(true, std::memory_order_release); });
  client.connect();
  (void)loop.runAfter(2s, [&loop] { loop.quit(); });
  loop.loop();

  serverThread.join();
  EXPECT_TRUE(gotBye.load(std::memory_order_acquire));
  EXPECT_TRUE(quitHandled.load(std::memory_order_acquire));
  EXPECT_TRUE(shutdownHandled.load(std::memory_order_acquire));
}

TEST_F(EchoClientTest, SequentialMultiClientConnect) {
  constexpr int kClients = 3;
  std::atomic<bool> serverReady{false};
  std::atomic<int> gotWorldCount{0};
  std::atomic<int> connectedCount{0};
  uint16_t port = 0;

  std::thread serverThread([&] {
    (void)startScriptedServerForMultiClient(port, serverReady, gotWorldCount, kClients);
  });

  const bool readyInTime = waitServerReady(serverReady);
  if (!readyInTime) {
    serverThread.join();
    FAIL() << "Raw multiclient server failed to start in time";
  }

  muduo::net::EventLoop loop;
  std::vector<std::unique_ptr<OriginalStyleEchoClient>> clients;
  clients.reserve(kClients);
  for (int i = 0; i < kClients; ++i) {
    clients.emplace_back(std::make_unique<OriginalStyleEchoClient>(
        &loop, muduo::net::InetAddress("127.0.0.1"sv, port),
        std::to_string(i + 1)));
  }

  int current = 0;
  for (int i = 0; i < kClients; ++i) {
    clients[static_cast<size_t>(i)]->setOnConnected([&, i] {
      connectedCount.fetch_add(1, std::memory_order_relaxed);
      if (i == current && i + 1 < kClients) {
        ++current;
        clients[static_cast<size_t>(current)]->connect();
      }
    });
  }

  clients[0]->connect();
  (void)loop.runAfter(1s, [&loop] { loop.quit(); });
  loop.loop();

  serverThread.join();
  EXPECT_EQ(connectedCount.load(std::memory_order_relaxed), kClients);
  EXPECT_EQ(gotWorldCount.load(std::memory_order_relaxed), kClients);
}
