#pragma once

#include "muduo/net/protorpc/RpcCodec.h"

#include <google/protobuf/service.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace muduo::net {

class RpcChannel : public ::google::protobuf::RpcChannel {
public:
  RpcChannel();
  explicit RpcChannel(const TcpConnectionPtr &conn);
  ~RpcChannel() override;

  void setConnection(const TcpConnectionPtr &conn) { conn_ = conn; }

  void setServices(
      const std::map<std::string, ::google::protobuf::Service *> *services) {
    services_ = services;
  }

  void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                  ::google::protobuf::RpcController *controller,
                  const ::google::protobuf::Message *request,
                  ::google::protobuf::Message *response,
                  ::google::protobuf::Closure *done) override;

  void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp receiveTime);

private:
  void onRpcMessage(const TcpConnectionPtr &conn, const RpcMessagePtr &messagePtr,
                    Timestamp receiveTime);

  void doneCallback(::google::protobuf::Message *response, int64_t id);

  struct NoDeleteMessage {
    void operator()(::google::protobuf::Message *) const noexcept {}
  };

  using ResponsePtr =
      std::shared_ptr<::google::protobuf::Message>;
  using DonePtr = std::unique_ptr<::google::protobuf::Closure>;

  struct OutstandingCall {
    ResponsePtr response;
    DonePtr done;
  };

  RpcCodec codec_;
  TcpConnectionPtr conn_;
  std::atomic<int64_t> id_{0};

  std::mutex mutex_;
  std::map<int64_t, OutstandingCall> outstandings_;

  const std::map<std::string, ::google::protobuf::Service *> *services_{nullptr};
};

using RpcChannelPtr = std::shared_ptr<RpcChannel>;

} // namespace muduo::net
