#include "muduo/net/Poller.h"

#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"

using namespace muduo::net;

Poller::Poller(EventLoop *loop) : ownerLoop_(loop) {}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel *channel) const {
  assertInLoopThread();
  const auto it = channels_.find(channel->fd());
  return it != channels_.end() && it->second == channel;
}

void Poller::assertInLoopThread() const { ownerLoop_->assertInLoopThread(); }
