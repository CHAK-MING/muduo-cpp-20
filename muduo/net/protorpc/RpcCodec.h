#pragma once

#include "muduo/net/protobuf/ProtobufCodecLite.h"

namespace muduo::net {

class RpcMessage;
using RpcMessagePtr = std::shared_ptr<RpcMessage>;

extern const char rpctag[];

using RpcCodec = ProtobufCodecLiteT<RpcMessage, rpctag>;

} // namespace muduo::net
