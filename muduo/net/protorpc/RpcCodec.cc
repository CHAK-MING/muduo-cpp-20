#include "muduo/net/protorpc/RpcCodec.h"

#include <google/protobuf/stubs/common.h>

namespace muduo::net {

[[maybe_unused]] const int kProtobufVersionCheck = [] {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  return 0;
}();

const char rpctag[] = "RPC0";

} // namespace muduo::net
