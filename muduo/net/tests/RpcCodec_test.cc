#include "muduo/net/Buffer.h"
#include "muduo/net/protobuf/ProtobufCodecLite.h"
#include "muduo/net/protorpc/RpcCodec.h"

#include <gtest/gtest.h>

#include "rpc.pb.h"

namespace muduo::net {
namespace {
void rpcMessageCallback(const TcpConnectionPtr &, const RpcMessagePtr &, Timestamp) {}

TEST(RpcCodecTest, EncodeDecodeMatchesExpectedWire) {
  RpcMessage message;
  message.set_type(REQUEST);
  message.set_id(2);

  const char wire[] = "\0\0\0\x13"
                      "RPC0"
                      "\x08\x01\x11\x02\0\0\0\0\0\0\0"
                      "\x0f\xef\x01\x32";
  const string expected(wire, sizeof(wire) - 1);

  Buffer buf1;
  Buffer buf2;
  string s1;
  string s2;

  {
    RpcCodec codec(rpcMessageCallback);
    codec.fillEmptyBuffer(&buf1, message);
    s1 = buf1.toStringPiece().as_string();
  }

  {
    MessagePtr gotMessage;
    ProtobufCodecLite codec(
        &RpcMessage::default_instance(), "RPC0",
        [&gotMessage](const TcpConnectionPtr &, const MessagePtr &msg, Timestamp) {
          gotMessage = msg;
        });
    codec.fillEmptyBuffer(&buf2, message);
    s2 = buf2.toStringPiece().as_string();

    codec.onMessage(TcpConnectionPtr(), &buf1, Timestamp::now());
    ASSERT_TRUE(static_cast<bool>(gotMessage));
    EXPECT_EQ(gotMessage->DebugString(), message.DebugString());
  }

  EXPECT_EQ(s1, s2);
  EXPECT_EQ(s1, expected);
}

TEST(RpcCodecTest, DecodeWithCustomTag) {
  RpcMessage message;
  message.set_type(REQUEST);
  message.set_id(42);

  Buffer buf;
  MessagePtr gotMessage;
  ProtobufCodecLite codec(
      &RpcMessage::default_instance(), "XYZ",
      [&gotMessage](const TcpConnectionPtr &, const MessagePtr &msg, Timestamp) {
        gotMessage = msg;
      });
  codec.fillEmptyBuffer(&buf, message);
  codec.onMessage(TcpConnectionPtr(), &buf, Timestamp::now());

  ASSERT_TRUE(static_cast<bool>(gotMessage));
  EXPECT_EQ(gotMessage->DebugString(), message.DebugString());
}

TEST(RpcCodecTest, DecodeFailsOnChecksumError) {
  RpcMessage message;
  message.set_type(REQUEST);
  message.set_id(9);

  Buffer buf;
  MessagePtr gotMessage;
  ProtobufCodecLite::ErrorCode gotError = ProtobufCodecLite::kNoError;
  ProtobufCodecLite codec(
      &RpcMessage::default_instance(), "RPC0",
      [&gotMessage](const TcpConnectionPtr &, const MessagePtr &msg, Timestamp) {
        gotMessage = msg;
      },
      {},
      [&gotError](const TcpConnectionPtr &, Buffer *, Timestamp,
                  ProtobufCodecLite::ErrorCode code) { gotError = code; });
  codec.fillEmptyBuffer(&buf, message);
  std::string corrupted = buf.toStringPiece().as_string();
  ASSERT_FALSE(corrupted.empty());
  corrupted.back() = static_cast<char>(corrupted.back() ^ 0x01);
  Buffer bad;
  bad.append(corrupted);

  codec.onMessage(TcpConnectionPtr(), &bad, Timestamp::now());
  EXPECT_EQ(gotError, ProtobufCodecLite::kCheckSumError);
  EXPECT_FALSE(static_cast<bool>(gotMessage));
}

TEST(RpcCodecTest, DecodeFailsOnUnknownTag) {
  RpcMessage message;
  message.set_type(REQUEST);
  message.set_id(7);

  Buffer buf;
  MessagePtr gotMessage;
  ProtobufCodecLite::ErrorCode gotError = ProtobufCodecLite::kNoError;
  ProtobufCodecLite encoder(
      &RpcMessage::default_instance(), "RPC0",
      [&gotMessage](const TcpConnectionPtr &, const MessagePtr &msg, Timestamp) {
        gotMessage = msg;
      });
  ProtobufCodecLite decoder(
      &RpcMessage::default_instance(), "XYZ0",
      [&gotMessage](const TcpConnectionPtr &, const MessagePtr &msg, Timestamp) {
        gotMessage = msg;
      },
      {},
      [&gotError](const TcpConnectionPtr &, Buffer *, Timestamp,
                  ProtobufCodecLite::ErrorCode code) { gotError = code; });
  encoder.fillEmptyBuffer(&buf, message);

  decoder.onMessage(TcpConnectionPtr(), &buf, Timestamp::now());
  EXPECT_EQ(gotError, ProtobufCodecLite::kUnknownMessageType);
  EXPECT_FALSE(static_cast<bool>(gotMessage));
}

TEST(RpcCodecTest, RawCallbackCanDropFrame) {
  RpcMessage message;
  message.set_type(REQUEST);
  message.set_id(11);

  Buffer buf;
  MessagePtr gotMessage;
  ProtobufCodecLite::ErrorCode gotError = ProtobufCodecLite::kNoError;
  int rawCount = 0;
  ProtobufCodecLite codec(
      &RpcMessage::default_instance(), "RPC0",
      [&gotMessage](const TcpConnectionPtr &, const MessagePtr &msg, Timestamp) {
        gotMessage = msg;
      },
      [&rawCount](const TcpConnectionPtr &, StringPiece, Timestamp) {
        ++rawCount;
        return false;
      },
      [&gotError](const TcpConnectionPtr &, Buffer *, Timestamp,
                  ProtobufCodecLite::ErrorCode code) { gotError = code; });
  codec.fillEmptyBuffer(&buf, message);

  codec.onMessage(TcpConnectionPtr(), &buf, Timestamp::now());

  EXPECT_EQ(rawCount, 1);
  EXPECT_FALSE(static_cast<bool>(gotMessage));
  EXPECT_EQ(gotError, ProtobufCodecLite::kNoError);
  EXPECT_EQ(buf.readableBytes(), 0U);
}

TEST(RpcCodecTest, RejectsInvalidLengthFrame) {
  Buffer buf;
  buf.appendInt32(ProtobufCodecLite::kMaxMessageLen + 1);
  std::string payload(16, 'x');
  buf.append(payload);

  MessagePtr gotMessage;
  ProtobufCodecLite::ErrorCode gotError = ProtobufCodecLite::kNoError;
  ProtobufCodecLite codec(
      &RpcMessage::default_instance(), "RPC0",
      [&gotMessage](const TcpConnectionPtr &, const MessagePtr &msg, Timestamp) {
        gotMessage = msg;
      },
      {},
      [&gotError](const TcpConnectionPtr &, Buffer *, Timestamp,
                  ProtobufCodecLite::ErrorCode code) { gotError = code; });
  codec.onMessage(TcpConnectionPtr(), &buf, Timestamp::now());
  EXPECT_EQ(gotError, ProtobufCodecLite::kInvalidLength);
  EXPECT_FALSE(static_cast<bool>(gotMessage));
}

} // namespace
} // namespace muduo::net
