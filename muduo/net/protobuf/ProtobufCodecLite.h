#pragma once

#include "muduo/base/Timestamp.h"
#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"
#if MUDUO_ENABLE_LEGACY_COMPAT
#include "muduo/base/StringPiece.h"
#endif

#include <concepts>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace google::protobuf {
class Message;
}

namespace muduo::net {

using MessagePtr = std::shared_ptr<::google::protobuf::Message>;

class ProtobufCodecLite : muduo::noncopyable {
public:
  static constexpr int kHeaderLen = sizeof(int32_t);
  static constexpr int kChecksumLen = sizeof(int32_t);
  static constexpr int kMaxMessageLen = 64 * 1024 * 1024;

  enum class ErrorCode : uint8_t {
    kNoError = 0,
    kInvalidLength,
    kCheckSumError,
    kInvalidNameLen,
    kUnknownMessageType,
    kParseError,
  };

  static constexpr ErrorCode kNoError = ErrorCode::kNoError;
  static constexpr ErrorCode kInvalidLength = ErrorCode::kInvalidLength;
  static constexpr ErrorCode kCheckSumError = ErrorCode::kCheckSumError;
  static constexpr ErrorCode kInvalidNameLen = ErrorCode::kInvalidNameLen;
  static constexpr ErrorCode kUnknownMessageType = ErrorCode::kUnknownMessageType;
  static constexpr ErrorCode kParseError = ErrorCode::kParseError;

#if MUDUO_ENABLE_LEGACY_COMPAT
  using RawMessagePayload = StringPiece;
#else
  using RawMessagePayload = std::string_view;
#endif
  using RawMessageCallback =
      CallbackFunction<bool(const TcpConnectionPtr &, RawMessagePayload,
                            Timestamp)>;
  using ProtobufMessageCallback =
      CallbackFunction<void(const TcpConnectionPtr &, const MessagePtr &,
                            Timestamp)>;
  using ErrorCallback =
      CallbackFunction<void(const TcpConnectionPtr &, Buffer *, Timestamp,
                            ErrorCode)>;

  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    std::string_view tagArg, ProtobufMessageCallback messageCb,
                    RawMessageCallback rawCb = {},
                    ErrorCallback errorCb = ErrorCallback{defaultErrorCallback})
      : prototype_(prototype), tag_(tagArg),
        messageCallback_(std::move(messageCb)), rawCb_(std::move(rawCb)),
        errorCallback_(std::move(errorCb)),
        kMinMessageLen(static_cast<int>(tagArg.size()) + kChecksumLen) {}

#if MUDUO_ENABLE_LEGACY_COMPAT
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    StringPiece tagArg, ProtobufMessageCallback messageCb,
                    RawMessageCallback rawCb = {},
                    ErrorCallback errorCb = ErrorCallback{defaultErrorCallback})
      : ProtobufCodecLite(prototype, tagArg.as_string_view(),
                          std::move(messageCb), std::move(rawCb),
                          std::move(errorCb)) {}

  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    const char *tagArg, ProtobufMessageCallback messageCb,
                    RawMessageCallback rawCb = {},
                    ErrorCallback errorCb = ErrorCallback{defaultErrorCallback})
      : ProtobufCodecLite(prototype,
                          std::string_view{tagArg == nullptr ? "" : tagArg},
                          std::move(messageCb),
                          std::move(rawCb), std::move(errorCb)) {}
#endif

  template <typename MessageCb, typename RawCb = RawMessageCallback,
            typename ErrCb = ErrorCallback>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             std::constructible_from<RawMessageCallback, RawCb> &&
             std::constructible_from<ErrorCallback, ErrCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    std::string_view tagArg, MessageCb &&messageCb,
                    RawCb &&rawCb, ErrCb &&errorCb)
      : ProtobufCodecLite(
            prototype, tagArg,
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback(std::forward<RawCb>(rawCb)),
            ErrorCallback(std::forward<ErrCb>(errorCb))) {}

#if MUDUO_ENABLE_LEGACY_COMPAT
  template <typename MessageCb, typename RawCb = RawMessageCallback,
            typename ErrCb = ErrorCallback>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             std::constructible_from<RawMessageCallback, RawCb> &&
             std::constructible_from<ErrorCallback, ErrCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    StringPiece tagArg, MessageCb &&messageCb, RawCb &&rawCb,
                    ErrCb &&errorCb)
      : ProtobufCodecLite(
            prototype, tagArg.as_string_view(),
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback(std::forward<RawCb>(rawCb)),
            ErrorCallback(std::forward<ErrCb>(errorCb))) {}

  template <typename MessageCb, typename RawCb = RawMessageCallback,
            typename ErrCb = ErrorCallback>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             std::constructible_from<RawMessageCallback, RawCb> &&
             std::constructible_from<ErrorCallback, ErrCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    const char *tagArg, MessageCb &&messageCb, RawCb &&rawCb,
                    ErrCb &&errorCb)
      : ProtobufCodecLite(
            prototype, std::string_view{tagArg == nullptr ? "" : tagArg},
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback(std::forward<RawCb>(rawCb)),
            ErrorCallback(std::forward<ErrCb>(errorCb))) {}
#endif

  template <typename MessageCb>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    std::string_view tagArg, MessageCb &&messageCb)
      : ProtobufCodecLite(
            prototype, tagArg,
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback{},
            ErrorCallback{defaultErrorCallback}) {}

#if MUDUO_ENABLE_LEGACY_COMPAT
  template <typename MessageCb>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    StringPiece tagArg, MessageCb &&messageCb)
      : ProtobufCodecLite(
            prototype, tagArg.as_string_view(),
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback{},
            ErrorCallback{defaultErrorCallback}) {}

  template <typename MessageCb>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    const char *tagArg, MessageCb &&messageCb)
      : ProtobufCodecLite(
            prototype, std::string_view{tagArg == nullptr ? "" : tagArg},
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback{},
            ErrorCallback{defaultErrorCallback}) {}
#endif

  template <typename MessageCb, typename RawCb>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             std::constructible_from<RawMessageCallback, RawCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    std::string_view tagArg, MessageCb &&messageCb,
                    RawCb &&rawCb)
      : ProtobufCodecLite(
            prototype, tagArg,
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback(std::forward<RawCb>(rawCb)),
            ErrorCallback{defaultErrorCallback}) {}

#if MUDUO_ENABLE_LEGACY_COMPAT
  template <typename MessageCb, typename RawCb>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             std::constructible_from<RawMessageCallback, RawCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    StringPiece tagArg, MessageCb &&messageCb, RawCb &&rawCb)
      : ProtobufCodecLite(
            prototype, tagArg.as_string_view(),
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback(std::forward<RawCb>(rawCb)),
            ErrorCallback{defaultErrorCallback}) {}

  template <typename MessageCb, typename RawCb>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             std::constructible_from<RawMessageCallback, RawCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  ProtobufCodecLite(const ::google::protobuf::Message *prototype,
                    const char *tagArg, MessageCb &&messageCb, RawCb &&rawCb)
      : ProtobufCodecLite(
            prototype, std::string_view{tagArg == nullptr ? "" : tagArg},
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback(std::forward<RawCb>(rawCb)),
            ErrorCallback{defaultErrorCallback}) {}
#endif

  virtual ~ProtobufCodecLite() = default;

  [[nodiscard]] const string &tag() const { return tag_; }

  void send(const TcpConnectionPtr &conn, const ::google::protobuf::Message &message);

  void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp receiveTime);

  [[nodiscard]] virtual bool parseFromBuffer(std::span<const std::byte> buf,
                                             ::google::protobuf::Message *message);
  [[nodiscard]] virtual bool parseFromBuffer(
      std::string_view buf, ::google::protobuf::Message *message);
#if MUDUO_ENABLE_LEGACY_COMPAT
  [[nodiscard]] virtual bool parseFromBuffer(
      StringPiece buf, ::google::protobuf::Message *message);
#endif
  [[nodiscard]] virtual int serializeToBuffer(
      const ::google::protobuf::Message &message, Buffer *buf);

  [[nodiscard]] static const string &errorCodeToString(ErrorCode errorCode);

  [[nodiscard]] ErrorCode parse(std::span<const std::byte> buf,
                                ::google::protobuf::Message *message);
  [[nodiscard]] ErrorCode parse(std::string_view buf,
                                ::google::protobuf::Message *message);
#if MUDUO_ENABLE_LEGACY_COMPAT
  [[nodiscard]] ErrorCode parse(const char *buf, int len,
                                ::google::protobuf::Message *message);
#endif
  void fillEmptyBuffer(Buffer *buf, const ::google::protobuf::Message &message);

  [[nodiscard]] static int32_t checksum(std::span<const std::byte> bytes);
  [[nodiscard]] static int32_t checksum(const void *buf, int len);
  [[nodiscard]] static bool validateChecksum(std::span<const std::byte> bytes);
  [[nodiscard]] static bool validateChecksum(std::string_view bytes);
#if MUDUO_ENABLE_LEGACY_COMPAT
  [[nodiscard]] static bool validateChecksum(const char *buf, int len);
#endif
  [[nodiscard]] static int32_t asInt32(std::span<const std::byte> bytes);
#if MUDUO_ENABLE_LEGACY_COMPAT
  [[nodiscard]] static int32_t asInt32(const char *buf);
#endif

  static void defaultErrorCallback(const TcpConnectionPtr &, Buffer *, Timestamp,
                                   ErrorCode);

private:
  const ::google::protobuf::Message *prototype_;
  const string tag_;
  ProtobufMessageCallback messageCallback_;
  RawMessageCallback rawCb_;
  ErrorCallback errorCallback_;
  const int kMinMessageLen;
};

template <typename MSG, const char *TAG, typename CODEC = ProtobufCodecLite>
  requires std::derived_from<CODEC, ProtobufCodecLite>
class ProtobufCodecLiteT {
public:
  using ConcreteMessagePtr = std::shared_ptr<MSG>;
  using ProtobufMessageCallback =
      CallbackFunction<void(const TcpConnectionPtr &, const ConcreteMessagePtr &,
                            Timestamp)>;
  using RawMessageCallback = ProtobufCodecLite::RawMessageCallback;
  using ErrorCallback = ProtobufCodecLite::ErrorCallback;

  explicit ProtobufCodecLiteT(ProtobufMessageCallback messageCb,
                              RawMessageCallback rawCb = {},
                              ErrorCallback errorCb =
                                  ErrorCallback{
                                      ProtobufCodecLite::defaultErrorCallback})
      : messageCallback_(std::move(messageCb)),
        codec_(&MSG::default_instance(), std::string_view{TAG},
               [this](const TcpConnectionPtr &conn, const MessagePtr &message,
                      Timestamp receiveTime) {
                 onRpcMessage(conn, message, receiveTime);
               },
               std::move(rawCb), std::move(errorCb)) {}

  template <typename MessageCb, typename RawCb = RawMessageCallback,
            typename ErrCb = ErrorCallback>
    requires std::constructible_from<ProtobufMessageCallback, MessageCb> &&
             std::constructible_from<RawMessageCallback, RawCb> &&
             std::constructible_from<ErrorCallback, ErrCb> &&
             (!std::same_as<std::remove_cvref_t<MessageCb>,
                            ProtobufMessageCallback>)
  explicit ProtobufCodecLiteT(MessageCb &&messageCb, RawCb &&rawCb = {},
                              ErrCb &&errorCb = ErrorCallback{
                                  ProtobufCodecLite::defaultErrorCallback})
      : ProtobufCodecLiteT(
            ProtobufMessageCallback(std::forward<MessageCb>(messageCb)),
            RawMessageCallback(std::forward<RawCb>(rawCb)),
            ErrorCallback(std::forward<ErrCb>(errorCb))) {}

  [[nodiscard]] const string &tag() const { return codec_.tag(); }

  void send(const TcpConnectionPtr &conn, const MSG &message) {
    codec_.send(conn, message);
  }

  void onMessage(const TcpConnectionPtr &conn, Buffer *buf,
                 Timestamp receiveTime) {
    codec_.onMessage(conn, buf, receiveTime);
  }

  void onRpcMessage(const TcpConnectionPtr &conn, const MessagePtr &message,
                    Timestamp receiveTime) {
    messageCallback_(conn, ::muduo::down_pointer_cast<MSG>(message), receiveTime);
  }

  void fillEmptyBuffer(Buffer *buf, const MSG &message) {
    codec_.fillEmptyBuffer(buf, message);
  }

private:
  ProtobufMessageCallback messageCallback_;
  CODEC codec_;
};

} // namespace muduo::net
