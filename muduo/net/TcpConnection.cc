#include "muduo/net/TcpConnection.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Socket.h"
#include "muduo/net/SocketsOps.h"

#include <array>
#include <cerrno>
#include <netinet/tcp.h>
#include <vector>

namespace muduo::net {

void defaultConnectionCallback(const TcpConnectionPtr &conn) {
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
}

void defaultMessageCallback([[maybe_unused]] const TcpConnectionPtr &conn,
                            Buffer *buffer,
                            [[maybe_unused]] Timestamp receiveTime) {
  buffer->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop *loop, string name, int sockfd,
                             InetAddress localAddr, InetAddress peerAddr)
    : loop_(muduo::CheckNotNull("loop", loop)), name_(std::move(name)),
      socket_(std::make_unique<Socket>(sockfd)),
      channel_(std::make_unique<Channel>(loop, sockfd)),
      localAddr_(std::move(localAddr)), peerAddr_(std::move(peerAddr)) {
  channel_->setReadCallback(
      [this](Timestamp receiveTime) { handleRead(receiveTime); });
  channel_->setWriteCallback([this] { handleWrite(); });
  channel_->setCloseCallback([this] { handleClose(); });
  channel_->setErrorCallback([this] { handleError(); });

  LOG_DEBUG << "TcpConnection::ctor[" << name_ << "] at " << this
            << " fd=" << sockfd;
  socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
  LOG_DEBUG << "TcpConnection::dtor[" << name_ << "] at " << this
            << " fd=" << channel_->fd() << " state=" << stateToString();
  assert(state_ == StateE::kDisconnected);
}

bool TcpConnection::getTcpInfo(tcp_info *tcpi) const {
  return socket_->getTcpInfo(tcpi);
}

string TcpConnection::getTcpInfoString() const {
  std::array<char, 1024> buf{};
  (void)socket_->getTcpInfoString(buf.data(), static_cast<int>(buf.size()));
  return string{buf.data()};
}

void TcpConnection::send(const void *message, int len) {
  send(std::as_bytes(std::span{static_cast<const char *>(message),
                               static_cast<size_t>(len)}));
}

void TcpConnection::send(const char *message) {
  send(std::string_view{message});
}

void TcpConnection::send(const string &message) {
  send(std::string_view{message});
}

void TcpConnection::send(string &&message) {
  send(std::string_view{message});
}

void TcpConnection::send(StringPiece message) {
  send(std::string_view{message.data(), message.size()});
}

void TcpConnection::send(std::string_view message) {
  send(std::as_bytes(std::span{message.data(), message.size()}));
}

void TcpConnection::send(std::span<const std::byte> message) {
  if (state_ != StateE::kConnected) {
    return;
  }
  if (loop_->isInLoopThread()) {
    sendInLoop(message);
    return;
  }

  std::vector<std::byte> data(message.begin(), message.end());
  const auto weakSelf = weak_from_this();
  loop_->runInLoop([weakSelf, data = std::move(data)] {
    if (const auto self = weakSelf.lock()) {
      self->sendInLoop(std::span<const std::byte>{data.data(), data.size()});
    }
  });
}

void TcpConnection::send(Buffer *message) {
  if (state_ != StateE::kConnected) {
    return;
  }

  if (loop_->isInLoopThread()) {
    sendInLoop(message->readableSpan());
    message->retrieveAll();
    return;
  }

  const auto readable = message->readableSpan();
  std::vector<std::byte> data(readable.begin(), readable.end());
  message->retrieveAll();
  const auto weakSelf = weak_from_this();
  loop_->runInLoop([weakSelf, data = std::move(data)] {
    if (const auto self = weakSelf.lock()) {
      self->sendInLoop(std::span<const std::byte>{data.data(), data.size()});
    }
  });
}

void TcpConnection::sendInLoop(std::span<const std::byte> message) {
  loop_->assertInLoopThread();

  if (state_ == StateE::kDisconnected) {
    LOG_WARN << "TcpConnection::sendInLoop disconnected, give up writing";
    return;
  }

  ssize_t nwrote = 0;
  size_t remaining = message.size();
  bool faultError = false;

  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
    nwrote = sockets::write(channel_->fd(), message);
    if (nwrote >= 0) {
      remaining = message.size() - static_cast<size_t>(nwrote);
      if (remaining == 0 && writeCompleteCallback_) {
        const auto weakSelf = weak_from_this();
        loop_->queueInLoop([weakSelf] {
          if (const auto self = weakSelf.lock();
              self && self->writeCompleteCallback_) {
            self->writeCompleteCallback_(self);
          }
        });
      }
    } else {
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG_SYSERR << "TcpConnection::sendInLoop";
        if (errno == EPIPE || errno == ECONNRESET) {
          faultError = true;
        }
      }
    }
  }

  if (!faultError && remaining > 0) {
    const size_t oldLen = outputBuffer_.readableBytes();
    if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ &&
        highWaterMarkCallback_) {
      const auto weakSelf = weak_from_this();
      const size_t totalLen = oldLen + remaining;
      loop_->queueInLoop([weakSelf, totalLen] {
        if (const auto self = weakSelf.lock();
            self && self->highWaterMarkCallback_) {
          self->highWaterMarkCallback_(self, totalLen);
        }
      });
    }

    outputBuffer_.append(
        message.subspan(static_cast<size_t>(nwrote), remaining));
    if (!channel_->isWriting()) {
      channel_->enableWriting();
    }
  }
}

void TcpConnection::shutdown() {
  if (state_ == StateE::kConnected) {
    setState(StateE::kDisconnecting);
    const auto weakSelf = weak_from_this();
    loop_->runInLoop([weakSelf] {
      if (const auto self = weakSelf.lock()) {
        self->shutdownInLoop();
      }
    });
  }
}

void TcpConnection::shutdownInLoop() {
  loop_->assertInLoopThread();
  if (!channel_->isWriting()) {
    socket_->shutdownWrite();
  }
}

void TcpConnection::forceClose() {
  if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting) {
    setState(StateE::kDisconnecting);
    auto self = shared_from_this();
    loop_->runInLoop([self = std::move(self)] { self->forceCloseInLoop(); });
  }
}

void TcpConnection::forceCloseWithDelay(double seconds) {
  if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting) {
    setState(StateE::kDisconnecting);
    const auto weakSelf = weak_from_this();
    (void)loop_->runAfter(seconds, [weakSelf] {
      if (const auto self = weakSelf.lock()) {
        self->forceClose();
      }
    });
  }
}

void TcpConnection::forceCloseInLoop() {
  loop_->assertInLoopThread();
  if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting) {
    handleClose();
  }
}

void TcpConnection::setTcpNoDelay(bool on) { socket_->setTcpNoDelay(on); }

void TcpConnection::startRead() {
  const auto weakSelf = weak_from_this();
  loop_->runInLoop([weakSelf] {
    if (const auto self = weakSelf.lock()) {
      self->startReadInLoop();
    }
  });
}

void TcpConnection::startReadInLoop() {
  loop_->assertInLoopThread();
  if (!reading_ || !channel_->isReading()) {
    channel_->enableReading();
    reading_ = true;
  }
}

void TcpConnection::stopRead() {
  const auto weakSelf = weak_from_this();
  loop_->runInLoop([weakSelf] {
    if (const auto self = weakSelf.lock()) {
      self->stopReadInLoop();
    }
  });
}

void TcpConnection::stopReadInLoop() {
  loop_->assertInLoopThread();
  if (reading_ || channel_->isReading()) {
    channel_->disableReading();
    reading_ = false;
  }
}

void TcpConnection::connectEstablished() {
  loop_->assertInLoopThread();
  assert(state_ == StateE::kConnecting);
  setState(StateE::kConnected);
  channel_->tie(shared_from_this());
  channel_->enableReading();
  reading_ = true;

  if (connectionCallback_) {
    connectionCallback_(shared_from_this());
  }
}

void TcpConnection::connectDestroyed() {
  loop_->assertInLoopThread();
  if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting) {
    setState(StateE::kDisconnected);
    channel_->disableAll();

    if (connectionCallback_) {
      connectionCallback_(shared_from_this());
    }
  }
  channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime) {
  loop_->assertInLoopThread();
  int savedErrno = 0;
  const ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0) {
    if (messageCallback_) {
      messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
  } else if (n == 0) {
    handleClose();
  } else {
    errno = savedErrno;
    LOG_SYSERR << "TcpConnection::handleRead";
    handleError();
  }
}

void TcpConnection::handleWrite() {
  loop_->assertInLoopThread();
  if (!channel_->isWriting()) {
    LOG_TRACE << "TcpConnection fd = " << channel_->fd()
              << " is down, no more writing";
    return;
  }

  const ssize_t n = sockets::write(channel_->fd(), outputBuffer_.readableSpan());
  if (n > 0) {
    outputBuffer_.retrieve(static_cast<size_t>(n));
    if (outputBuffer_.readableBytes() == 0) {
      channel_->disableWriting();
      if (writeCompleteCallback_) {
        const auto weakSelf = weak_from_this();
        loop_->queueInLoop([weakSelf] {
          if (const auto self = weakSelf.lock();
              self && self->writeCompleteCallback_) {
            self->writeCompleteCallback_(self);
          }
        });
      }
      if (state_ == StateE::kDisconnecting) {
        shutdownInLoop();
      }
    }
  } else {
    LOG_SYSERR << "TcpConnection::handleWrite";
  }
}

void TcpConnection::handleClose() {
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpConnection fd = " << channel_->fd()
            << " state = " << stateToString();
  assert(state_ == StateE::kConnected || state_ == StateE::kDisconnecting);

  setState(StateE::kDisconnected);
  channel_->disableAll();

  TcpConnectionPtr guardThis(shared_from_this());
  if (connectionCallback_) {
    connectionCallback_(guardThis);
  }
  if (closeCallback_) {
    closeCallback_(guardThis);
  }
}

void TcpConnection::handleError() {
  const int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR << "TcpConnection::handleError [" << name_ << "] - SO_ERROR = "
            << err << " " << strerror_tl(err);
}

const char *TcpConnection::stateToString() const {
  switch (state_) {
  case StateE::kDisconnected:
    return "kDisconnected";
  case StateE::kConnecting:
    return "kConnecting";
  case StateE::kConnected:
    return "kConnected";
  case StateE::kDisconnecting:
    return "kDisconnecting";
  default:
    return "unknown";
  }
}

} // namespace muduo::net
