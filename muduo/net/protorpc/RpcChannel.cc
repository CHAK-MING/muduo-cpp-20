#include "muduo/net/protorpc/RpcChannel.h"

#include "muduo/base/Logging.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/stubs/callback.h>

#include "rpc.pb.h"

namespace muduo::net {

  RpcChannel::RpcChannel()
    : codec_([this](const TcpConnectionPtr &conn, const RpcMessagePtr &msg,
                    Timestamp receiveTime) {
        onRpcMessage(conn, msg, receiveTime);
      }) {
  muduo::logInfo("RpcChannel::ctor - {}", static_cast<const void *>(this));
}

RpcChannel::RpcChannel(const TcpConnectionPtr &conn)
    : codec_([this](const TcpConnectionPtr &c, const RpcMessagePtr &msg,
                    Timestamp receiveTime) {
        onRpcMessage(c, msg, receiveTime);
      }),
      conn_(conn) {
  muduo::logInfo("RpcChannel::ctor - {}", static_cast<const void *>(this));
}

RpcChannel::~RpcChannel() {
  muduo::logInfo("RpcChannel::dtor - {}", static_cast<const void *>(this));
}

void RpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                            ::google::protobuf::RpcController *,
                            const ::google::protobuf::Message *request,
                            ::google::protobuf::Message *response,
                            ::google::protobuf::Closure *done) {
  RpcMessage message;
  message.set_type(REQUEST);
  const int64_t id = id_.fetch_add(1, std::memory_order_relaxed) + 1;
  message.set_id(static_cast<uint64_t>(id));
  message.set_service(method->service()->full_name());
  message.set_method(method->name());
  message.set_request(request->SerializeAsString());

  {
    auto responseRef = ResponsePtr(response, NoDeleteMessage{});
    std::scoped_lock lock(mutex_);
    auto [_, inserted] = outstandings_.try_emplace(
        id, OutstandingCall{
                std::move(responseRef),
                DonePtr(done),
            });
    assert(inserted);
  }

  codec_.send(conn_, message);
}

void RpcChannel::onMessage(const TcpConnectionPtr &conn, Buffer *buf,
                           Timestamp receiveTime) {
  codec_.onMessage(conn, buf, receiveTime);
}

void RpcChannel::onRpcMessage(const TcpConnectionPtr &conn,
                              const RpcMessagePtr &messagePtr, Timestamp) {
  if (conn != conn_) {
    return;
  }

  RpcMessage &message = *messagePtr;
  if (message.type() == RESPONSE) {
    const int64_t id = static_cast<int64_t>(message.id());
    OutstandingCall out{};

    {
      std::scoped_lock lock(mutex_);
      if (const auto it = outstandings_.find(id); it != outstandings_.end()) {
        out = std::move(it->second);
        outstandings_.erase(it);
      }
    }

    if (out.response) {
      if (message.has_response()) {
        out.response->ParseFromString(message.response());
      }
      if (out.done) {
        auto *done = out.done.release();
        done->Run();
      }
    }
    return;
  }

  if (message.type() == REQUEST) {
    ErrorCode error = WRONG_PROTO;

    if (services_ != nullptr) {
      if (const auto svcIt = services_->find(message.service());
          svcIt != services_->end()) {
        auto *service = svcIt->second;
        const auto *desc = service->GetDescriptor();
        if (const auto *method = desc->FindMethodByName(message.method());
            method != nullptr) {
          std::unique_ptr<::google::protobuf::Message> request(
              service->GetRequestPrototype(method).New());
          if (request->ParseFromString(message.request())) {
            auto *response = service->GetResponsePrototype(method).New();
            const int64_t id = static_cast<int64_t>(message.id());
            service->CallMethod(
                method, nullptr, request.get(), response,
                ::google::protobuf::NewCallback(this, &RpcChannel::doneCallback,
                                                response, id));
            error = NO_ERROR;
          } else {
            error = INVALID_REQUEST;
          }
        } else {
          error = NO_METHOD;
        }
      } else {
        error = NO_SERVICE;
      }
    } else {
      error = NO_SERVICE;
    }

    if (error != NO_ERROR) {
      RpcMessage response;
      response.set_type(RESPONSE);
      response.set_id(message.id());
      response.set_error(error);
      codec_.send(conn_, response);
    }
  }
}

void RpcChannel::doneCallback(::google::protobuf::Message *response, int64_t id) {
  std::unique_ptr<::google::protobuf::Message> holder(response);

  RpcMessage message;
  message.set_type(RESPONSE);
  message.set_id(static_cast<uint64_t>(id));
  message.set_response(response->SerializeAsString());
  codec_.send(conn_, message);
}

} // namespace muduo::net
