#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpServer.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/Logging.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <chrono>
#include <string_view>

#include <sys/socket.h>
#include <unistd.h>

namespace {
using namespace std::string_view_literals;

class TcpClientRegressionTest : public ::testing::Test {};

} // namespace

TEST_F(TcpClientRegressionTest, StopInSameEventIteration) {
  using namespace std::chrono_literals;

  muduo::net::EventLoop loop;
  muduo::net::InetAddress serverAddr("127.0.0.1"sv, 2);
  muduo::net::TcpClient client(&loop, serverAddr, "TcpClient");

  (void)loop.runAfter(0ms, [&client] { client.stop(); });
  (void)loop.runAfter(300ms, [&loop] { loop.quit(); });
  client.connect();
  muduo::CurrentThread::sleepUsec(100 * 1000);
  loop.loop();
  SUCCEED();
}

TEST_F(TcpClientRegressionTest, DestructInDifferentThread) {
  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::DEBUG);
  muduo::net::EventLoopThread loopThread;
  {
    muduo::net::InetAddress serverAddr("127.0.0.1"sv, 1234);
    muduo::net::TcpClient client(loopThread.startLoop(), serverAddr,
                                 "TcpClient");
    client.connect();
    muduo::CurrentThread::sleepUsec(300 * 1000);
    client.disconnect();
  }
  muduo::CurrentThread::sleepUsec(500 * 1000);
  SUCCEED();
}

TEST_F(TcpClientRegressionTest, DestructWhenConnectedAndUnique) {
  using namespace std::chrono_literals;

  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::DEBUG);
  muduo::net::EventLoop loop;
  const auto port = static_cast<uint16_t>(32000 + (::getpid() % 10000));
  muduo::net::InetAddress listenAddr(port, true);
  muduo::net::TcpServer server(&loop, listenAddr, "TcpClientReg2Server");
  server.setConnectionCallback([](const muduo::net::TcpConnectionPtr &) {});
  server.setMessageCallback([](const muduo::net::TcpConnectionPtr &,
                               muduo::net::Buffer *, muduo::Timestamp) {});
  server.start();

  std::unique_ptr<muduo::net::TcpClient> client;
  std::atomic<bool> connected{false};

  (void)loop.runAfter(50ms, [&] {
    client = std::make_unique<muduo::net::TcpClient>(
        &loop, muduo::net::InetAddress("127.0.0.1"sv, port),
        "TcpClientReg2");
    client->setConnectionCallback(
        [&](const muduo::net::TcpConnectionPtr &conn) {
          if (conn->connected()) {
            connected.store(true, std::memory_order_release);
            client.reset();
          }
        });
    client->connect();
  });
  (void)loop.runAfter(1s, [&loop] { loop.quit(); });
  loop.loop();

  EXPECT_TRUE(connected.load(std::memory_order_acquire));
}
