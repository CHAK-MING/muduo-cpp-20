#pragma once

#include "muduo/net/TcpServer.h"

#include <map>
#include <string>

namespace google::protobuf {
class Service;
}

namespace muduo::net {

class RpcServer {
public:
  RpcServer(EventLoop *loop, const InetAddress &listenAddr);

  void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }

  void registerService(::google::protobuf::Service *service);
  void start();

private:
  void onConnection(const TcpConnectionPtr &conn);

  TcpServer server_;
  std::map<std::string, ::google::protobuf::Service *> services_;
};

} // namespace muduo::net
