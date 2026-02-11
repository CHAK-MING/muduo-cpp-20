#include "muduo/net/protobuf/ProtobufCodecLite.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Endian.h"
#include "muduo/net/TcpConnection.h"

#include <google/protobuf/message.h>
#include <zlib.h>

#include <array>
#include <bit>
#include <cassert>
#include <cstring>

namespace muduo::net {
namespace {

[[maybe_unused]] const int kProtobufVersion = [] {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  return 0;
}();

const string kNoErrorStr = "NoError";
const string kInvalidLengthStr = "InvalidLength";
const string kCheckSumErrorStr = "CheckSumError";
const string kInvalidNameLenStr = "InvalidNameLen";
const string kUnknownMessageTypeStr = "UnknownMessageType";
const string kParseErrorStr = "ParseError";
const string kUnknownErrorStr = "UnknownError";

} // namespace

void ProtobufCodecLite::send(const TcpConnectionPtr &conn,
                             const ::google::protobuf::Message &message) {
  Buffer buf;
  fillEmptyBuffer(&buf, message);
  conn->send(&buf);
}

void ProtobufCodecLite::fillEmptyBuffer(
    Buffer *buf, const ::google::protobuf::Message &message) {
  assert(buf->readableBytes() == 0);

  buf->append(std::string_view(tag_));
  const int byteSize = serializeToBuffer(message, buf);

  const int32_t checkSum =
      checksum(buf->readableSpan().first(buf->readableBytes()));
  buf->appendInt32(checkSum);

  assert(buf->readableBytes() == tag_.size() + static_cast<size_t>(byteSize) +
                                     static_cast<size_t>(kChecksumLen));

  const int32_t len =
      sockets::hostToNetwork32(static_cast<int32_t>(buf->readableBytes()));
  buf->prepend(&len, sizeof(len));
}

void ProtobufCodecLite::onMessage(const TcpConnectionPtr &conn, Buffer *buf,
                                  Timestamp receiveTime) {
  while (buf->readableBytes() >=
         static_cast<size_t>(kMinMessageLen + kHeaderLen)) {
    const int32_t len = buf->peekInt32();
    if (len > kMaxMessageLen || len < kMinMessageLen) {
      errorCallback_(conn, buf, receiveTime, ErrorCode::kInvalidLength);
      break;
    }

    const auto frameLen = static_cast<size_t>(kHeaderLen + len);
    if (buf->readableBytes() < frameLen) {
      break;
    }

    if (rawCb_ &&
        !rawCb_(conn, StringPiece(buf->readableChars().data(), frameLen),
                receiveTime)) {
      buf->retrieve(frameLen);
      continue;
    }

    MessagePtr message(prototype_->New());
    const ErrorCode errorCode =
        parse(buf->readableSpan().subspan(static_cast<size_t>(kHeaderLen),
                                          static_cast<size_t>(len)),
              message.get());
    if (errorCode == ErrorCode::kNoError) {
      messageCallback_(conn, message, receiveTime);
      buf->retrieve(frameLen);
    } else {
      errorCallback_(conn, buf, receiveTime, errorCode);
      break;
    }
  }
}

bool ProtobufCodecLite::parseFromBuffer(std::span<const std::byte> buf,
                                        ::google::protobuf::Message *message) {
  return message->ParseFromArray(buf.data(), static_cast<int>(buf.size()));
}

bool ProtobufCodecLite::parseFromBuffer(StringPiece buf,
                                        ::google::protobuf::Message *message) {
  return parseFromBuffer(
      std::as_bytes(std::span{buf.data(), static_cast<size_t>(buf.size())}),
      message);
}

int ProtobufCodecLite::serializeToBuffer(
    const ::google::protobuf::Message &message, Buffer *buf) {
  assert(message.IsInitialized());

  const int byteSize =
      ::google::protobuf::internal::ToIntSize(message.ByteSizeLong());
  buf->ensureWritableBytes(static_cast<size_t>(byteSize + kChecksumLen));

  auto *start = reinterpret_cast<uint8_t *>(buf->beginWrite());
  auto *end = message.SerializeWithCachedSizesToArray(start);
  const int written = static_cast<int>(end - start);
  if (written != byteSize) {
    LOG_ERROR << "serialize size mismatch expected=" << byteSize
              << " written=" << written;
  }
  buf->hasWritten(static_cast<size_t>(written));
  return written;
}

const string &ProtobufCodecLite::errorCodeToString(ErrorCode errorCode) {
  switch (errorCode) {
  case ErrorCode::kNoError:
    return kNoErrorStr;
  case ErrorCode::kInvalidLength:
    return kInvalidLengthStr;
  case ErrorCode::kCheckSumError:
    return kCheckSumErrorStr;
  case ErrorCode::kInvalidNameLen:
    return kInvalidNameLenStr;
  case ErrorCode::kUnknownMessageType:
    return kUnknownMessageTypeStr;
  case ErrorCode::kParseError:
    return kParseErrorStr;
  default:
    return kUnknownErrorStr;
  }
}

void ProtobufCodecLite::defaultErrorCallback(const TcpConnectionPtr &conn,
                                             Buffer *, Timestamp,
                                             ErrorCode errorCode) {
  LOG_ERROR << "ProtobufCodecLite::defaultErrorCallback - "
            << errorCodeToString(errorCode);
  if (conn && conn->connected()) {
    conn->shutdown();
  }
}

int32_t ProtobufCodecLite::asInt32(const char *buf) {
  int32_t be32 = 0;
  std::memcpy(&be32, buf, sizeof(be32));
  return sockets::networkToHost32(be32);
}

int32_t ProtobufCodecLite::checksum(const void *buf, int len) {
  return checksum(std::as_bytes(
      std::span{static_cast<const char *>(buf), static_cast<size_t>(len)}));
}

int32_t ProtobufCodecLite::checksum(std::span<const std::byte> bytes) {
  return static_cast<int32_t>(
      ::adler32(1, reinterpret_cast<const Bytef *>(bytes.data()),
                static_cast<uInt>(bytes.size())));
}

bool ProtobufCodecLite::validateChecksum(const char *buf, int len) {
  return validateChecksum(
      std::as_bytes(std::span{buf, static_cast<size_t>(len)}));
}

bool ProtobufCodecLite::validateChecksum(std::span<const std::byte> bytes) {
  if (bytes.size() < static_cast<size_t>(kChecksumLen)) {
    return false;
  }
  const char *raw = reinterpret_cast<const char *>(bytes.data());
  const auto len = static_cast<std::ptrdiff_t>(bytes.size());
  const int32_t expectedCheckSum = asInt32(raw + len - kChecksumLen);
  const int32_t checkSum = checksum(bytes.first(bytes.size() - kChecksumLen));
  return checkSum == expectedCheckSum;
}

ProtobufCodecLite::ErrorCode
ProtobufCodecLite::parse(const char *buf, int len,
                         ::google::protobuf::Message *message) {
  return parse(std::as_bytes(std::span{buf, static_cast<size_t>(len)}),
               message);
}

ProtobufCodecLite::ErrorCode
ProtobufCodecLite::parse(std::span<const std::byte> buf,
                         ::google::protobuf::Message *message) {
  if (!validateChecksum(buf)) {
    return ErrorCode::kCheckSumError;
  }

  const auto raw = reinterpret_cast<const char *>(buf.data());
  if (buf.size() < tag_.size() + static_cast<size_t>(kChecksumLen) ||
      std::memcmp(raw, tag_.data(), tag_.size()) != 0) {
    return ErrorCode::kUnknownMessageType;
  }

  const auto data =
      buf.subspan(tag_.size(), buf.size() - tag_.size() - kChecksumLen);
  if (parseFromBuffer(data, message)) {
    return ErrorCode::kNoError;
  }
  return ErrorCode::kParseError;
}

} // namespace muduo::net
