#include "muduo/net/TcpServer.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Acceptor.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/SocketsOps.h"

#include <format>
#include <utility>

namespace muduo::net {
namespace {

template <typename Callback, typename... Args>
void invokeCallbackIfSet(const std::shared_ptr<Callback> &cb, Args &&...args) {
  if (cb != nullptr && static_cast<bool>(*cb)) {
    (*cb)(std::forward<Args>(args)...);
  }
}

} // namespace

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr,
                     string nameArg, Option option)
    : loop_(muduo::CheckNotNull("loop", loop)), ipPort_(listenAddr.toIpPort()),
      name_(std::move(nameArg)),
      acceptor_(std::make_unique<Acceptor>(
          loop, listenAddr, option == Option::kReusePort)),
      threadPool_(std::make_shared<EventLoopThreadPool>(loop, name_)),
      connectionCallback_(std::make_shared<ConnectionCallback>(
          ConnectionCallback(defaultConnectionCallback))),
      messageCallback_(std::make_shared<MessageCallback>(
          MessageCallback(defaultMessageCallback))) {
  acceptor_->setNewConnectionCallback(
      [this](int sockfd, const InetAddress &peerAddr) {
        newConnection(sockfd, peerAddr);
      });
}

TcpServer::~TcpServer() {
  loop_->assertInLoopThread();
  muduo::logTrace("TcpServer::~TcpServer [{}] destructing", name_);

  for (auto &[_, conn] : connections_) {
    TcpConnectionPtr guard(conn);
    conn.reset();
    guard->getLoop()->runInLoop([guard] { guard->connectDestroyed(); });
  }
}

void TcpServer::setThreadNum(int numThreads) {
  assert(numThreads >= 0);
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
  if (started_.fetch_add(1, std::memory_order_acq_rel) == 0) {
    threadPool_->start(std::move(threadInitCallback_));
    assert(!acceptor_->listening());
    loop_->runInLoop([this] { acceptor_->listen(); });
  }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
  loop_->assertInLoopThread();
  EventLoop *ioLoop = threadPool_->getNextLoop();

  auto connName =
      std::format("{}-{}#{}", name_, ipPort_, static_cast<long>(nextConnId_));
  ++nextConnId_;

  muduo::logInfo("TcpServer::newConnection [{}] - new connection [{}] from {}",
                 name_, connName, peerAddr.toIpPort());

  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  auto conn = std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr,
                                              peerAddr);
  connections_[connName] = conn;

  auto connectionCb = connectionCallback_;
  auto messageCb = messageCallback_;
  auto writeCompleteCb = writeCompleteCallback_;
  conn->setConnectionCallback([connectionCb](const TcpConnectionPtr &c) {
    invokeCallbackIfSet(connectionCb, c);
  });
  conn->setMessageCallback(
      [messageCb](const TcpConnectionPtr &c, Buffer *b, Timestamp t) {
        invokeCallbackIfSet(messageCb, c, b, t);
      });
  conn->setWriteCompleteCallback([writeCompleteCb](const TcpConnectionPtr &c) {
    invokeCallbackIfSet(writeCompleteCb, c);
  });
  conn->setCloseCallback(
      [this](const TcpConnectionPtr &c) { removeConnection(c); });

  ioLoop->runInLoop([conn] { conn->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
  loop_->runInLoop([this, conn] { removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
  loop_->assertInLoopThread();
  muduo::logInfo("TcpServer::removeConnectionInLoop [{}] - connection {}", name_,
                 conn->name());

  const size_t erased = connections_.erase(conn->name());
  (void)erased;
  assert(erased == 1);

  EventLoop *ioLoop = conn->getLoop();
  ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
}

} // namespace muduo::net
