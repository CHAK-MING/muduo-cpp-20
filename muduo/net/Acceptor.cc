#include "muduo/net/Acceptor.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace muduo::net {

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr,
                   bool reusePort)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),
      acceptChannel_(loop, acceptSocket_.fd()),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
  assert(idleFd_ >= 0);

  acceptSocket_.setReuseAddr(true);
  acceptSocket_.setReusePort(reusePort);
  acceptSocket_.bindAddress(listenAddr);

  acceptChannel_.setReadCallback([this](Timestamp) { handleRead(); });
}

Acceptor::~Acceptor() {
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idleFd_);
}

void Acceptor::listen() {
  loop_->assertInLoopThread();
  listening_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
  loop_->assertInLoopThread();

  InetAddress peerAddr;
  const int connfd = acceptSocket_.accept(&peerAddr);
  if (connfd >= 0) {
    if (newConnectionCallback_) {
      newConnectionCallback_(connfd, peerAddr);
    } else {
      sockets::close(connfd);
    }
    return;
  }

  LOG_SYSERR << "Acceptor::handleRead";

  if (errno == EMFILE) {
    ::close(idleFd_);
    idleFd_ = ::accept(acceptSocket_.fd(), nullptr, nullptr);
    ::close(idleFd_);
    idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
  }
}

} // namespace muduo::net
