#pragma once

#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/TcpConnection.h"

#include <atomic>
#include <concepts>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace muduo::net {

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

class TcpServer : muduo::noncopyable {
public:
  using ThreadInitCallback = CallbackFunction<void(EventLoop *)>;
  enum class Option { kNoReusePort, kReusePort };

  TcpServer(EventLoop *loop, const InetAddress &listenAddr, string nameArg,
            Option option = Option::kNoReusePort);
  ~TcpServer();

  [[nodiscard]] const string &ipPort() const { return ipPort_; }
  [[nodiscard]] const string &name() const { return name_; }
  [[nodiscard]] EventLoop *getLoop() const { return loop_; }

  void setThreadNum(int numThreads);
  void setThreadInitCallback(ThreadInitCallback cb) {
    threadInitCallback_ = std::move(cb);
  }
  template <typename F>
    requires CallbackBindable<F, ThreadInitCallback>
  void setThreadInitCallback(F &&cb) {
    threadInitCallback_ = ThreadInitCallback(std::forward<F>(cb));
  }
  [[nodiscard]] std::shared_ptr<EventLoopThreadPool> threadPool() {
    return threadPool_;
  }

  void start();

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
  void newConnection(int sockfd, const InetAddress &peerAddr);
  void removeConnection(const TcpConnectionPtr &conn);
  void removeConnectionInLoop(const TcpConnectionPtr &conn);

  using ConnectionMap = std::unordered_map<string, TcpConnectionPtr>;

  EventLoop *loop_;
  const string ipPort_;
  const string name_;
  std::unique_ptr<Acceptor> acceptor_;
  std::shared_ptr<EventLoopThreadPool> threadPool_;

  std::shared_ptr<ConnectionCallback> connectionCallback_;
  std::shared_ptr<MessageCallback> messageCallback_;
  std::shared_ptr<WriteCompleteCallback> writeCompleteCallback_;
  ThreadInitCallback threadInitCallback_;
  std::atomic<int> started_{0};
  int nextConnId_{1};
  ConnectionMap connections_;
};

} // namespace muduo::net
