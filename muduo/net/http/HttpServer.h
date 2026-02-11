#pragma once

#include "muduo/net/Callbacks.h"
#include "muduo/net/TcpServer.h"

#include <concepts>
#include <type_traits>

namespace muduo::net {

class HttpRequest;
class HttpResponse;

class HttpServer {
public:
  using HttpCallback =
      CallbackFunction<void(const HttpRequest &, HttpResponse *)>;

  HttpServer(EventLoop *loop, const InetAddress &listenAddr, const string &name,
             TcpServer::Option option = TcpServer::Option::kNoReusePort);

  [[nodiscard]] EventLoop *getLoop() const { return server_.getLoop(); }

  template <typename F>
    requires CallbackBindable<F, HttpCallback>
  void setHttpCallback(F &&cb) {
    httpCallback_ = HttpCallback(std::forward<F>(cb));
  }

  void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }

  void start();

private:
  void onConnection(const TcpConnectionPtr &conn);
  void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp receiveTime);
  void onRequest(const TcpConnectionPtr &conn, const HttpRequest &req);

  TcpServer server_;
  HttpCallback httpCallback_;
};

} // namespace muduo::net
