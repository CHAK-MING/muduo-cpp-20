#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/inspect/Inspector.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace muduo::net {
namespace {

int pickPort() {
  static std::atomic<int> port{36000 + (::getpid() % 20000)};
  return port.fetch_add(1, std::memory_order_relaxed);
}

std::string httpGet(uint16_t port, std::string_view path) {
  const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return {};
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
    ::close(fd);
    return {};
  }
  if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return {};
  }

  std::string request = "GET " + std::string(path) +
                        " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
  size_t sent = 0;
  while (sent < request.size()) {
    const ssize_t n = ::write(fd, request.data() + sent, request.size() - sent);
    if (n <= 0) {
      break;
    }
    sent += static_cast<size_t>(n);
  }

  std::string response;
  std::array<char, 2048> buf{};
  while (true) {
    const ssize_t n = ::read(fd, buf.data(), buf.size());
    if (n <= 0) {
      break;
    }
    response.append(buf.data(), static_cast<size_t>(n));
  }
  ::close(fd);
  return response;
}

TEST(InspectorTest, ServesHttpEndpoints) {
  using namespace std::chrono_literals;

  EventLoop loop;
  const uint16_t port = static_cast<uint16_t>(pickPort());

  Inspector inspector(&loop, InetAddress(port), "gtest");

  std::string indexResp;
  std::string pidResp;
  std::thread client([&indexResp, &loop, &pidResp, port] {
    std::this_thread::sleep_for(120ms);
    indexResp = httpGet(port, "/");
    pidResp = httpGet(port, "/proc/pid");
    loop.quit();
  });

  (void)loop.runAfter(2s, [&loop] { loop.quit(); });
  loop.loop();
  client.join();

  ASSERT_FALSE(indexResp.empty());
  EXPECT_NE(indexResp.find("200 OK"), std::string::npos);
  EXPECT_NE(indexResp.find("/proc/overview"), std::string::npos);

  ASSERT_FALSE(pidResp.empty());
  EXPECT_NE(pidResp.find("200 OK"), std::string::npos);
}

TEST(InspectorTest, ServesProcessAndSystemPages) {
  using namespace std::chrono_literals;

  EventLoop loop;
  const uint16_t port = static_cast<uint16_t>(pickPort());
  Inspector inspector(&loop, InetAddress(port), "gtest-full");

  std::array<std::string, 9> responses;
  std::thread client([&loop, &responses, port] {
    std::this_thread::sleep_for(120ms);
    responses[0] = httpGet(port, "/proc/overview");
    responses[1] = httpGet(port, "/proc/status");
    responses[2] = httpGet(port, "/proc/threads");
    responses[3] = httpGet(port, "/sys/overview");
    responses[4] = httpGet(port, "/sys/loadavg");
    responses[5] = httpGet(port, "/sys/version");
    responses[6] = httpGet(port, "/sys/cpuinfo");
    responses[7] = httpGet(port, "/sys/meminfo");
    responses[8] = httpGet(port, "/sys/stat");
    loop.quit();
  });

  (void)loop.runAfter(3s, [&loop] { loop.quit(); });
  loop.loop();
  client.join();

  for (const auto &resp : responses) {
    ASSERT_FALSE(resp.empty());
    EXPECT_NE(resp.find("200 OK"), std::string::npos);
  }

  EXPECT_NE(responses[0].find("Opened files"), std::string::npos);
  EXPECT_NE(responses[1].find("Threads"), std::string::npos);
  EXPECT_NE(responses[2].find("TID NAME"), std::string::npos);
  EXPECT_NE(responses[3].find("Boot time"), std::string::npos);
  EXPECT_NE(responses[4].find('.'), std::string::npos);
  EXPECT_NE(responses[5].find("Linux"), std::string::npos);
  EXPECT_NE(responses[6].find("processor"), std::string::npos);
  EXPECT_NE(responses[7].find("MemTotal"), std::string::npos);
  EXPECT_NE(responses[8].find("cpu"), std::string::npos);
}

} // namespace
} // namespace muduo::net
