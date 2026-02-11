#include "muduo/net/Channel.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), logHup_(true),
      tied_(false), eventHandling_(false), addedToLoop_(false) {}

Channel::~Channel() {
  assert(!eventHandling_);
  assert(!addedToLoop_);
}

void Channel::tie(const std::shared_ptr<void> &obj) {
  tie_ = obj;
  tied_ = true;
}

void Channel::update() {
  addedToLoop_ = true;
  loop_->updateChannel(this);
}

void Channel::remove() {
  assert(isNoneEvent());
  addedToLoop_ = false;
  loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
  std::shared_ptr<void> guard;
  if (tied_) {
    guard = tie_.lock();
    if (guard) {
      handleEventWithGuard(receiveTime);
    }
    return;
  }
  handleEventWithGuard(receiveTime);
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
  eventHandling_ = true;
  muduo::logTrace("{}", reventsToString());

  const bool hasInvalidEvent = (revents_ & POLLNVAL) != 0;
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
    if (logHup_) {
      muduo::logWarn("fd = {} Channel::handleEvent() POLLHUP", fd_);
    }
    if (closeCallback_) {
      closeCallback_();
    }
  }

  if (hasInvalidEvent) {
    muduo::logWarn("fd = {} Channel::handleEvent() POLLNVAL", fd_);
  }

  if ((revents_ & POLLERR) || hasInvalidEvent) {
    if (errorCallback_) {
      errorCallback_();
    }
  }

  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
    if (readCallback_) {
      readCallback_(receiveTime);
    }
  }

  if (revents_ & POLLOUT) {
    if (writeCallback_) {
      writeCallback_();
    }
  }

  eventHandling_ = false;
}

void Channel::enableReading() {
  events_ |= kReadEvent;
  update();
}

void Channel::disableReading() {
  events_ &= ~kReadEvent;
  update();
}

void Channel::enableWriting() {
  events_ |= kWriteEvent;
  update();
}

void Channel::disableWriting() {
  events_ &= ~kWriteEvent;
  update();
}

void Channel::disableAll() {
  events_ = kNoneEvent;
  update();
}

std::string Channel::reventsToString() const {
  return eventsToString(fd_, revents_);
}

std::string Channel::eventsToString() const {
  return eventsToString(fd_, events_);
}

std::string Channel::eventsToString(int fd, int events) {
  std::string result;
  result.reserve(48);
  result.append(std::to_string(fd));
  result.append(": ");
  if (events & POLLIN)
    result.append("IN ");
  if (events & POLLPRI)
    result.append("PRI ");
  if (events & POLLOUT)
    result.append("OUT ");
  if (events & POLLHUP)
    result.append("HUP ");
  if (events & POLLRDHUP)
    result.append("RDHUP ");
  if (events & POLLERR)
    result.append("ERR ");
  if (events & POLLNVAL)
    result.append("NVAL ");
  return result;
}
