#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpResponse.h"
#include "muduo/net/http/HttpServer.h"

#include <cstdlib>
#include <string_view>

namespace muduo::net {
namespace {

bool benchmarkMode = false;

void onRequest(const HttpRequest &req, HttpResponse *resp) {
  if (req.path() == "/" || req.path() == "/hello") {
    resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/plain");
    resp->addHeader("Server", "muduo-cpp20");
    resp->setBody("hello, world!\n");
    return;
  }

  if (!benchmarkMode) {
    LOG_WARN << "Not found path=" << req.path();
  }
  resp->setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
  resp->setStatusMessage("Not Found");
  resp->setCloseConnection(true);
}

} // namespace
} // namespace muduo::net

int main(int argc, char *argv[]) {
  using muduo::Logger;
  using muduo::net::EventLoop;
  using muduo::net::HttpServer;
  using muduo::net::InetAddress;

  int numThreads = 0;
  int port = 8000;
  if (argc > 1) {
    muduo::net::benchmarkMode = true;
    Logger::setLogLevel(Logger::WARN);
    numThreads = std::atoi(argv[1]);
  }
  if (argc > 2) {
    port = std::atoi(argv[2]);
  }

  EventLoop loop;
  HttpServer server(&loop, InetAddress(static_cast<uint16_t>(port)), "http-bench");
  server.setHttpCallback(muduo::net::onRequest);
  server.setThreadNum(numThreads);
  server.start();
  loop.loop();
  return 0;
}
