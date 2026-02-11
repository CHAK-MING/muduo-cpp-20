#include "muduo/net/Connector.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/SocketsOps.h"

#include <algorithm>
#include <cerrno>

namespace muduo::net {

Connector::Connector(EventLoop *loop, const InetAddress &serverAddr)
    : loop_(loop), serverAddr_(serverAddr) {
  LOG_DEBUG << "Connector ctor[" << this << "]";
}

Connector::~Connector() {
  LOG_DEBUG << "Connector dtor[" << this << "]";
  if (channel_) {
    if (loop_->isInLoopThread()) {
      channel_->disableAll();
      channel_->remove();
    }
    channel_.reset();
  }
}

void Connector::start() {
  connect_.store(true, std::memory_order_release);
  const auto weakSelf = weak_from_this();
  loop_->runInLoop([weakSelf] {
    if (const auto self = weakSelf.lock()) {
      self->startInLoop();
    }
  });
}

void Connector::restart() {
  loop_->assertInLoopThread();
  setState(States::kDisconnected);
  retryDelayMs_ = kInitRetryDelayMs;
  connect_.store(true, std::memory_order_release);
  startInLoop();
}

void Connector::stop() {
  connect_.store(false, std::memory_order_release);
  const auto weakSelf = weak_from_this();
  loop_->queueInLoop([weakSelf] {
    if (const auto self = weakSelf.lock()) {
      self->stopInLoop();
    }
  });
}

void Connector::startInLoop() {
  loop_->assertInLoopThread();
  assert(state_ == States::kDisconnected);
  if (connect_.load(std::memory_order_acquire)) {
    connect();
  } else {
    LOG_DEBUG << "Connector::startInLoop do not connect";
  }
}

void Connector::stopInLoop() {
  loop_->assertInLoopThread();
  if (state_ == States::kConnecting) {
    setState(States::kDisconnected);
    const int sockfd = removeAndResetChannel();
    sockets::close(sockfd);
  }
}

void Connector::connect() {
  const int sockfd = sockets::createNonblockingOrDie(serverAddr_.family());
  const int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());
  const int savedErrno = (ret == 0) ? 0 : errno;

  switch (savedErrno) {
  case 0:
  case EINPROGRESS:
  case EINTR:
  case EISCONN:
    connecting(sockfd);
    break;

  case EAGAIN:
  case EADDRINUSE:
  case EADDRNOTAVAIL:
  case ECONNREFUSED:
  case ENETUNREACH:
    retry(sockfd);
    break;

  case EACCES:
  case EPERM:
  case EAFNOSUPPORT:
  case EALREADY:
  case EBADF:
  case EFAULT:
  case ENOTSOCK:
    LOG_SYSERR << "Connector::connect error " << savedErrno;
    sockets::close(sockfd);
    break;

  default:
    LOG_SYSERR << "Connector::connect unexpected error " << savedErrno;
    sockets::close(sockfd);
    break;
  }
}

void Connector::connecting(int sockfd) {
  setState(States::kConnecting);
  assert(!channel_);
  channel_ = std::make_unique<Channel>(loop_, sockfd);

  const auto weakSelf = weak_from_this();
  channel_->setWriteCallback([weakSelf] {
    if (const auto self = weakSelf.lock()) {
      self->handleWrite();
    }
  });
  channel_->setErrorCallback([weakSelf] {
    if (const auto self = weakSelf.lock()) {
      self->handleError();
    }
  });
  channel_->enableWriting();
}

int Connector::removeAndResetChannel() {
  channel_->disableAll();
  channel_->remove();
  const int sockfd = channel_->fd();

  auto self = shared_from_this();
  loop_->queueInLoop([self = std::move(self)] { self->resetChannel(); });
  return sockfd;
}

void Connector::resetChannel() { channel_.reset(); }

void Connector::handleWrite() {
  LOG_TRACE << "Connector::handleWrite state=" << static_cast<int>(state_);

  if (state_ != States::kConnecting) {
    assert(state_ == States::kDisconnected);
    return;
  }

  const int sockfd = removeAndResetChannel();
  const int err = sockets::getSocketError(sockfd);
  if (err != 0) {
    LOG_WARN << "Connector::handleWrite - SO_ERROR = " << err << " "
             << muduo::strerror_tl(err);
    retry(sockfd);
    return;
  }

  if (sockets::isSelfConnect(sockfd)) {
    LOG_WARN << "Connector::handleWrite - Self connect";
    retry(sockfd);
    return;
  }

  setState(States::kConnected);
  if (connect_.load(std::memory_order_acquire)) {
    if (newConnectionCallback_) {
      newConnectionCallback_(sockfd);
    } else {
      sockets::close(sockfd);
    }
  } else {
    sockets::close(sockfd);
  }
}

void Connector::handleError() {
  LOG_ERROR << "Connector::handleError state=" << static_cast<int>(state_);
  if (state_ == States::kConnecting) {
    const int sockfd = removeAndResetChannel();
    const int err = sockets::getSocketError(sockfd);
    LOG_TRACE << "Connector::handleError SO_ERROR = " << err << " "
              << muduo::strerror_tl(err);
    retry(sockfd);
  }
}

void Connector::retry(int sockfd) {
  sockets::close(sockfd);
  setState(States::kDisconnected);

  if (!connect_.load(std::memory_order_acquire)) {
    LOG_DEBUG << "Connector::retry do not connect";
    return;
  }

  LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
           << " in " << retryDelayMs_ << " milliseconds";

  const auto delaySec = static_cast<double>(retryDelayMs_) / 1000.0;
  const auto weakSelf = weak_from_this();
  (void)loop_->runAfter(delaySec, [weakSelf] {
    if (const auto self = weakSelf.lock()) {
      self->startInLoop();
    }
  });
  retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
}

} // namespace muduo::net
