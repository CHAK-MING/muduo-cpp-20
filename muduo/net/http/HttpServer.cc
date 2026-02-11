#include "muduo/net/http/HttpServer.h"

#include "muduo/base/Logging.h"
#include "muduo/net/http/HttpContext.h"
#include "muduo/net/http/HttpRequest.h"
#include "muduo/net/http/HttpResponse.h"

#include <any>

namespace muduo::net {
namespace detail {

void defaultHttpCallback(const HttpRequest &, HttpResponse *resp) {
  resp->setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
  resp->setStatusMessage("Not Found");
  resp->setCloseConnection(true);
}

} // namespace detail

HttpServer::HttpServer(EventLoop *loop, const InetAddress &listenAddr,
                       const string &name, TcpServer::Option option)
    : server_(loop, listenAddr, name, option),
      httpCallback_(HttpCallback(detail::defaultHttpCallback)) {
  server_.setConnectionCallback([this](const TcpConnectionPtr &conn) {
    onConnection(conn);
  });
  server_.setMessageCallback(
      [this](const TcpConnectionPtr &conn, Buffer *buf, Timestamp receiveTime) {
        onMessage(conn, buf, receiveTime);
      });
}

void HttpServer::start() {
  muduo::logWarn("HttpServer[{}] starts listening on {}", server_.name(),
                 server_.ipPort());
  server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr &conn) {
  if (conn->connected()) {
    conn->setContext(HttpContext{});
  }
}

void HttpServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf,
                           Timestamp receiveTime) {
  auto *context = std::any_cast<HttpContext>(conn->getMutableContext());
  if (context == nullptr) {
    conn->setContext(HttpContext{});
    context = std::any_cast<HttpContext>(conn->getMutableContext());
  }
  if (context == nullptr) {
    conn->send(std::string_view("HTTP/1.1 500 Internal Server Error\r\n\r\n"));
    conn->shutdown();
    return;
  }

  if (!context->parseRequest(buf, receiveTime)) {
    conn->send(std::string_view("HTTP/1.1 400 Bad Request\r\n\r\n"));
    conn->shutdown();
    return;
  }

  if (context->gotAll()) {
    onRequest(conn, context->request());
    context->reset();
  }
}

void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req) {
  const string connection = req.getHeader("Connection");
  const bool close = connection == "close" ||
                     (req.getVersion() == HttpRequest::Version::kHttp10 &&
                      connection != "Keep-Alive");

  HttpResponse response(close);
  httpCallback_(req, &response);

  Buffer buf;
  response.appendToBuffer(&buf);
  conn->send(&buf);
  if (response.closeConnection()) {
    conn->shutdown();
  }
}

} // namespace muduo::net
