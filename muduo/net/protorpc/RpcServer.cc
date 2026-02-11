#include "muduo/net/protorpc/RpcServer.h"

#include "muduo/base/Logging.h"
#include "muduo/net/protorpc/RpcChannel.h"

#include <any>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>

namespace muduo::net {

RpcServer::RpcServer(EventLoop *loop, const InetAddress &listenAddr)
    : server_(loop, listenAddr, "RpcServer") {
  server_.setConnectionCallback(
      [this](const TcpConnectionPtr &conn) { onConnection(conn); });
}

void RpcServer::registerService(::google::protobuf::Service *service) {
  const auto *desc = service->GetDescriptor();
  services_[std::string(desc->full_name())] = service;
}

void RpcServer::start() { server_.start(); }

void RpcServer::onConnection(const TcpConnectionPtr &conn) {
  LOG_INFO << "RpcServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected()) {
    auto channel = std::make_shared<RpcChannel>(conn);
    channel->setServices(&services_);
    conn->setMessageCallback(
        [channel](const TcpConnectionPtr &c, Buffer *buf, Timestamp t) {
          channel->onMessage(c, buf, t);
        });
    conn->setContext(channel);
  } else {
    conn->setContext(RpcChannelPtr{});
  }
}

} // namespace muduo::net
