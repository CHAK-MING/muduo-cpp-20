#pragma once

#include "muduo/base/StringPiece.h"
#include "muduo/base/noncopyable.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/InetAddress.h"

#include <any>
#include <concepts>
#include <memory>
#include <span>
#include <string_view>

struct tcp_info;

namespace muduo::net {

class Channel;
class EventLoop;
class Socket;

class TcpConnection : muduo::noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
  TcpConnection(EventLoop *loop, string name, int sockfd, InetAddress localAddr,
                InetAddress peerAddr);
  ~TcpConnection();

  [[nodiscard]] EventLoop *getLoop() const { return loop_; }
  [[nodiscard]] const string &name() const { return name_; }
  [[nodiscard]] const InetAddress &localAddress() const { return localAddr_; }
  [[nodiscard]] const InetAddress &peerAddress() const { return peerAddr_; }
  [[nodiscard]] bool connected() const { return state_ == StateE::kConnected; }
  [[nodiscard]] bool disconnected() const {
    return state_ == StateE::kDisconnected;
  }
  [[nodiscard]] bool getTcpInfo(tcp_info *tcpi) const;
  [[nodiscard]] string getTcpInfoString() const;

  void send(const void *message, int len);
  void send(const char *message);
  void send(const string &message);
  void send(string &&message);
  void send(StringPiece message);
  void send(std::string_view message);
  void send(std::span<const std::byte> message);
  void send(Buffer *message);

  void shutdown();
  void forceClose();
  void forceCloseWithDelay(double seconds);
  void setTcpNoDelay(bool on);

  void startRead();
  void stopRead();
  [[nodiscard]] bool isReading() const { return reading_; }

  void setContext(std::any context) { context_ = std::move(context); }
  [[nodiscard]] const std::any &getContext() const { return context_; }
  [[nodiscard]] std::any *getMutableContext() { return &context_; }

  template <typename F>
    requires CallbackBindable<F, ConnectionCallback>
  void setConnectionCallback(F &&cb) {
    connectionCallback_ = ConnectionCallback(std::forward<F>(cb));
  }
  void setConnectionCallback(ConnectionCallback cb) {
    connectionCallback_ = std::move(cb);
  }

  template <typename F>
    requires CallbackBindable<F, MessageCallback>
  void setMessageCallback(F &&cb) {
    messageCallback_ = MessageCallback(std::forward<F>(cb));
  }
  void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }

  template <typename F>
    requires CallbackBindable<F, WriteCompleteCallback>
  void setWriteCompleteCallback(F &&cb) {
    writeCompleteCallback_ = WriteCompleteCallback(std::forward<F>(cb));
  }
  void setWriteCompleteCallback(WriteCompleteCallback cb) {
    writeCompleteCallback_ = std::move(cb);
  }

  template <typename F>
    requires CallbackBindable<F, HighWaterMarkCallback>
  void setHighWaterMarkCallback(F &&cb, size_t highWaterMark) {
    highWaterMarkCallback_ = HighWaterMarkCallback(std::forward<F>(cb));
    highWaterMark_ = highWaterMark;
  }
  void setHighWaterMarkCallback(HighWaterMarkCallback cb, size_t highWaterMark) {
    highWaterMarkCallback_ = std::move(cb);
    highWaterMark_ = highWaterMark;
  }

  template <typename F>
    requires CallbackBindable<F, CloseCallback>
  void setCloseCallback(F &&cb) {
    closeCallback_ = CloseCallback(std::forward<F>(cb));
  }
  void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

  [[nodiscard]] Buffer *inputBuffer() { return &inputBuffer_; }
  [[nodiscard]] Buffer *outputBuffer() { return &outputBuffer_; }

  void connectEstablished();
  void connectDestroyed();

private:
  enum class StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();
  void sendInLoop(std::span<const std::byte> message);
  void shutdownInLoop();
  void forceCloseInLoop();
  void startReadInLoop();
  void stopReadInLoop();
  void setState(StateE state) { state_ = state; }
  [[nodiscard]] const char *stateToString() const;

  EventLoop *loop_;
  const string name_;
  StateE state_{StateE::kConnecting};
  bool reading_{false};
  std::unique_ptr<Socket> socket_;
  std::unique_ptr<Channel> channel_;
  const InetAddress localAddr_;
  const InetAddress peerAddr_;

  ConnectionCallback connectionCallback_{ConnectionCallback(defaultConnectionCallback)};
  MessageCallback messageCallback_{MessageCallback(defaultMessageCallback)};
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;
  CloseCallback closeCallback_;
  size_t highWaterMark_{64 * 1024 * 1024};
  Buffer inputBuffer_;
  Buffer outputBuffer_;
  std::any context_;
};

} // namespace muduo::net
