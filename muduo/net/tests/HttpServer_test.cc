#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpResponse.h"
#include "muduo/net/http/HttpServer.h"

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
  static std::atomic<int> port{38000 + (::getpid() % 20000)};
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

void onRequest(const HttpRequest &req, HttpResponse *resp) {
  if (req.path() == "/") {
    resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/html");
    resp->addHeader("Server", "Muduo");
    resp->setBody("<html><body><h1>Hello</h1></body></html>");
  } else if (req.path() == "/hello") {
    resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/plain");
    resp->addHeader("Server", "Muduo");
    resp->setBody("hello, world!\n");
  } else {
    resp->setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
  }
}

TEST(HttpServerTest, ServesHelloAndNotFound) {
  using namespace std::chrono_literals;

  EventLoop loop;
  const uint16_t port = static_cast<uint16_t>(pickPort());

  HttpServer server(&loop, InetAddress(port), "http-test");
  server.setHttpCallback(onRequest);
  server.start();

  std::string hello;
  std::string notFound;
  std::thread client([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    hello = httpGet(port, "/hello");
    notFound = httpGet(port, "/missing");
    loop.quit();
  });

  (void)loop.runAfter(2s, [&loop] { loop.quit(); });
  loop.loop();
  client.join();

  ASSERT_FALSE(hello.empty());
  EXPECT_NE(hello.find("200 OK"), std::string::npos);
  EXPECT_NE(hello.find("hello, world!"), std::string::npos);

  ASSERT_FALSE(notFound.empty());
  EXPECT_NE(notFound.find("404 Not Found"), std::string::npos);
}

} // namespace
} // namespace muduo::net
