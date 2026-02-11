#pragma once

#include "muduo/base/noncopyable.h"
#include "muduo/net/Connector.h"
#include "muduo/net/TcpConnection.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <type_traits>

namespace muduo::net {

class TcpClient : muduo::noncopyable {
public:
  TcpClient(EventLoop *loop, const InetAddress &serverAddr, string nameArg);
  ~TcpClient();

  void connect();
  void disconnect();
  void stop();

  [[nodiscard]] TcpConnectionPtr connection() const {
    std::scoped_lock lock(mutex_);
    return connection_;
  }

  [[nodiscard]] EventLoop *getLoop() const { return loop_; }
  [[nodiscard]] bool retry() const { return retry_.load(std::memory_order_acquire); }
  void enableRetry() { retry_.store(true, std::memory_order_release); }

  [[nodiscard]] const string &name() const { return name_; }

  template <typename F>
    requires CallbackBindable<F, ConnectionCallback>
  void setConnectionCallback(F &&cb) {
    connectionCallback_ = std::make_shared<ConnectionCallback>(
        ConnectionCallback(std::forward<F>(cb)));
  }

  template <typename F>
    requires CallbackBindable<F, MessageCallback>
  void setMessageCallback(F &&cb) {
    messageCallback_ = std::make_shared<MessageCallback>(
        MessageCallback(std::forward<F>(cb)));
  }

  template <typename F>
    requires CallbackBindable<F, WriteCompleteCallback>
  void setWriteCompleteCallback(F &&cb) {
    writeCompleteCallback_ = std::make_shared<WriteCompleteCallback>(
        WriteCompleteCallback(std::forward<F>(cb)));
  }

private:
  void newConnection(int sockfd);
  void removeConnection(const TcpConnectionPtr &conn);

  EventLoop *loop_;
  std::shared_ptr<Connector> connector_;
  const string name_;

  std::shared_ptr<ConnectionCallback> connectionCallback_;
  std::shared_ptr<MessageCallback> messageCallback_;
  std::shared_ptr<WriteCompleteCallback> writeCompleteCallback_;
  std::atomic<bool> retry_{false};
  std::atomic<bool> connect_{true};
  int nextConnId_{1};
  mutable std::mutex mutex_;
  TcpConnectionPtr connection_;
};

} // namespace muduo::net
