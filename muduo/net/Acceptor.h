#pragma once

#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/Channel.h"
#include "muduo/net/Socket.h"

#include <concepts>
#include <type_traits>

namespace muduo::net {

class EventLoop;
class InetAddress;

class Acceptor : muduo::noncopyable {
public:
  using NewConnectionCallback =
      CallbackFunction<void(int sockfd, const InetAddress &)>;

  Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reusePort);
  ~Acceptor();

  template <typename F>
    requires CallbackBindable<F, NewConnectionCallback>
  void setNewConnectionCallback(F &&cb) {
    newConnectionCallback_ = NewConnectionCallback(std::forward<F>(cb));
  }

  void listen();

  [[nodiscard]] bool listening() const noexcept { return listening_; }

private:
  void handleRead();

  EventLoop *loop_;
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
  bool listening_{false};
  int idleFd_;
};

} // namespace muduo::net
