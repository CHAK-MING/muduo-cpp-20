#include "muduo/net/Poller.h"

#include "muduo/net/poller/EPollPoller.h"
#include "muduo/net/poller/PollPoller.h"

#include <cstdlib>

using namespace muduo::net;

std::unique_ptr<Poller> Poller::newDefaultPoller(EventLoop *loop) {
  if (::getenv("MUDUO_USE_POLL") != nullptr) {
    return std::make_unique<PollPoller>(loop);
  }
  return std::make_unique<EPollPoller>(loop);
}
