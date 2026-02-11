#include "muduo/net/TcpClient.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/SocketsOps.h"

#include <chrono>
#include <format>
#include <utility>

namespace muduo::net {
using namespace std::chrono_literals;

namespace detail {

template <typename Callback, typename... Args>
void invokeCallbackIfSet(const std::shared_ptr<Callback> &cb, Args &&...args) {
  if (cb != nullptr && static_cast<bool>(*cb)) {
    (*cb)(std::forward<Args>(args)...);
  }
}

void removeConnection(EventLoop *loop, const TcpConnectionPtr &conn) {
  loop->runInLoop([conn] { conn->connectDestroyed(); });
}

void removeConnector([[maybe_unused]] const std::shared_ptr<Connector> &connector) {
  // Intentionally empty: keep connector alive until delayed callback fires.
}

} // namespace detail

TcpClient::TcpClient(EventLoop *loop, const InetAddress &serverAddr, string nameArg)
    : loop_(muduo::CheckNotNull("loop", loop)),
      connector_(std::make_shared<Connector>(loop, serverAddr)),
      name_(std::move(nameArg)),
      connectionCallback_(std::make_shared<ConnectionCallback>(
          ConnectionCallback(defaultConnectionCallback))),
      messageCallback_(std::make_shared<MessageCallback>(
          MessageCallback(defaultMessageCallback))) {
  connector_->setNewConnectionCallback([this](int sockfd) { newConnection(sockfd); });

  muduo::logInfo("TcpClient::TcpClient[{}] - connector {}", name_,
                 static_cast<const void *>(get_pointer(connector_)));
}

TcpClient::~TcpClient() {
  muduo::logInfo("TcpClient::~TcpClient[{}] - connector {}", name_,
                 static_cast<const void *>(get_pointer(connector_)));
  connector_->stop();

  TcpConnectionPtr conn;
  {
    std::scoped_lock lock(mutex_);
    conn = connection_;
  }

  if (conn) {
    assert(loop_ == conn->getLoop());
    if (loop_->isInLoopThread()) {
      conn->setCloseCallback(CloseCallback{});
      conn->connectDestroyed();
    } else {
      loop_->runInLoop([conn, loop = loop_] {
        conn->setCloseCallback(CloseCallback([loop](const TcpConnectionPtr &c) {
          detail::removeConnection(loop, c);
        }));
      });
      conn->forceClose();
    }
  } else {
    (void)loop_->runAfter(1s, [connector = connector_] {
      detail::removeConnector(connector);
    });
  }
}

void TcpClient::connect() {
  muduo::logInfo("TcpClient::connect[{}] - connecting to {}", name_,
                 connector_->serverAddress().toIpPort());
  connect_.store(true, std::memory_order_release);
  connector_->start();
}

void TcpClient::disconnect() {
  connect_.store(false, std::memory_order_release);
  std::scoped_lock lock(mutex_);
  if (connection_) {
    connection_->shutdown();
  }
}

void TcpClient::stop() {
  connect_.store(false, std::memory_order_release);
  connector_->stop();
}

void TcpClient::newConnection(int sockfd) {
  loop_->assertInLoopThread();
  const InetAddress peerAddr(sockets::getPeerAddr(sockfd));
  const string connName =
      std::format("{}:{}#{}", name_, peerAddr.toIpPort(), nextConnId_++);

  const InetAddress localAddr(sockets::getLocalAddr(sockfd));
  auto conn = std::make_shared<TcpConnection>(loop_, connName, sockfd, localAddr,
                                              peerAddr);

  auto connectionCb = connectionCallback_;
  auto messageCb = messageCallback_;
  auto writeCompleteCb = writeCompleteCallback_;
  conn->setConnectionCallback([connectionCb](const TcpConnectionPtr &c) {
    detail::invokeCallbackIfSet(connectionCb, c);
  });
  conn->setMessageCallback(
      [messageCb](const TcpConnectionPtr &c, Buffer *b, Timestamp t) {
        detail::invokeCallbackIfSet(messageCb, c, b, t);
      });
  conn->setWriteCompleteCallback([writeCompleteCb](const TcpConnectionPtr &c) {
    detail::invokeCallbackIfSet(writeCompleteCb, c);
  });
  conn->setCloseCallback([this](const TcpConnectionPtr &c) { removeConnection(c); });

  {
    std::scoped_lock lock(mutex_);
    connection_ = conn;
  }
  conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr &conn) {
  loop_->assertInLoopThread();
  assert(loop_ == conn->getLoop());

  {
    std::scoped_lock lock(mutex_);
    assert(connection_ == conn);
    connection_.reset();
  }

  loop_->runInLoop([conn] { conn->connectDestroyed(); });
  if (retry_.load(std::memory_order_acquire) &&
      connect_.load(std::memory_order_acquire)) {
    muduo::logInfo("TcpClient::removeConnection[{}] - reconnecting to {}", name_,
                   connector_->serverAddress().toIpPort());
    connector_->restart();
  }
}

} // namespace muduo::net
