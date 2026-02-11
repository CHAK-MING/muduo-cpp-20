#pragma once

#include "muduo/base/noncopyable.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/InetAddress.h"

#include <atomic>
#include <concepts>
#include <memory>
#include <type_traits>

namespace muduo::net {

class Channel;
class EventLoop;

class Connector : muduo::noncopyable,
                  public std::enable_shared_from_this<Connector> {
public:
  using NewConnectionCallback = CallbackFunction<void(int sockfd)>;

  Connector(EventLoop *loop, const InetAddress &serverAddr);
  ~Connector();

  template <typename F>
    requires CallbackBindable<F, NewConnectionCallback>
  void setNewConnectionCallback(F &&cb) {
    newConnectionCallback_ = NewConnectionCallback(std::forward<F>(cb));
  }

  void start();
  void restart();
  void stop();

  [[nodiscard]] const InetAddress &serverAddress() const noexcept {
    return serverAddr_;
  }

private:
  enum class States { kDisconnected, kConnecting, kConnected };

  static constexpr int kMaxRetryDelayMs = 30 * 1000;
  static constexpr int kInitRetryDelayMs = 500;

  void setState(States state) noexcept { state_ = state; }

  void startInLoop();
  void stopInLoop();
  void connect();
  void connecting(int sockfd);
  void handleWrite();
  void handleError();
  void retry(int sockfd);
  [[nodiscard]] int removeAndResetChannel();
  void resetChannel();

  EventLoop *loop_;
  InetAddress serverAddr_;
  std::atomic<bool> connect_{false};
  States state_{States::kDisconnected};
  std::unique_ptr<Channel> channel_;
  NewConnectionCallback newConnectionCallback_;
  int retryDelayMs_{kInitRetryDelayMs};
};

} // namespace muduo::net
