#include "muduo/net/poller/EPollPoller.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Types.h"
#include "muduo/net/Channel.h"

#include <cerrno>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

static_assert(EPOLLIN == POLLIN, "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI, "epoll uses same flag values as poll");
static_assert(EPOLLOUT == POLLOUT, "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP, "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR, "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP, "epoll uses same flag values as poll");

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
  if (epollfd_ < 0) {
    muduo::logSysFatal("EPollPoller::EPollPoller");
  }
}

EPollPoller::~EPollPoller() { ::close(epollfd_); }

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  muduo::logTrace("fd total count {}", channels_.size());

  int numEvents = ::epoll_wait(epollfd_, events_.data(),
                               static_cast<int>(events_.size()), timeoutMs);
  const int savedErrno = errno;
  const Timestamp now(Timestamp::now());

  if (numEvents > 0) {
    muduo::logTrace("{} events happened", numEvents);
    fillActiveChannels(numEvents, activeChannels);
    if (static_cast<size_t>(numEvents) == events_.size()) {
      events_.resize(events_.size() * 2);
    }
    return now;
  }

  if (numEvents == 0) {
    muduo::logTrace("nothing happened");
    return now;
  }

  if (savedErrno != EINTR) {
    errno = savedErrno;
    muduo::logSysErr("EPollPoller::poll()");
  }
  return now;
}

void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList *activeChannels) const {
  assert(static_cast<size_t>(numEvents) <= events_.size());
  activeChannels->reserve(activeChannels->size() +
                          static_cast<size_t>(numEvents));
  for (int i = 0; i < numEvents; ++i) {
    auto *channel = static_cast<Channel *>(events_[i].data.ptr);
#ifndef NDEBUG
    const int fd = channel->fd();
    const auto it = channels_.find(fd);
    assert(it != channels_.end());
    assert(it->second == channel);
#endif
    channel->setRevents(static_cast<int>(events_[i].events));
    activeChannels->push_back(channel);
  }
}

void EPollPoller::updateChannel(Channel *channel) {
  Poller::assertInLoopThread();
  const int index = channel->index();

  muduo::logTrace("fd = {} events = {} index = {}", channel->fd(),
                  channel->events(), index);

  if (index == kNew || index == kDeleted) {
    const int fd = channel->fd();
    if (index == kNew) {
      channels_[fd] = channel;
    }

    channel->setIndex(kAdded);
    update(EPOLL_CTL_ADD, channel);
    return;
  }

  const int fd = channel->fd();
  (void)fd;
  assert(channels_.contains(fd));
  assert(channels_[fd] == channel);
  assert(index == kAdded);

  if (channel->isNoneEvent()) {
    update(EPOLL_CTL_DEL, channel);
    channel->setIndex(kDeleted);
    return;
  }

  update(EPOLL_CTL_MOD, channel);
}

void EPollPoller::removeChannel(Channel *channel) {
  Poller::assertInLoopThread();

  const int fd = channel->fd();
  muduo::logTrace("fd = {}", fd);

  assert(channels_.contains(fd));
  assert(channels_[fd] == channel);
  assert(channel->isNoneEvent());

  const int index = channel->index();
  assert(index == kAdded || index == kDeleted);

  [[maybe_unused]] const auto erased = channels_.erase(fd);
  assert(erased == 1);

  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->setIndex(kNew);
}

void EPollPoller::update(int operation, Channel *channel) const {
  epoll_event event{};
  event.events = static_cast<uint32_t>(channel->events());
  event.data.ptr = channel;

  const int fd = channel->fd();
  muduo::logTrace("epoll_ctl op = {} fd = {} event = {{ {} }}",
                  operationToString(operation), fd, channel->eventsToString());

  if (::epoll_ctl(epollfd_, operation, fd, &event) == 0) {
    return;
  }

  if (operation == EPOLL_CTL_DEL) {
    muduo::logSysErr("epoll_ctl op ={} fd ={}", operationToString(operation),
                     fd);
    return;
  }
  muduo::logSysFatal("epoll_ctl op ={} fd ={}", operationToString(operation),
                     fd);
}

const char *EPollPoller::operationToString(int op) {
  switch (op) {
  case EPOLL_CTL_ADD:
    return "ADD";
  case EPOLL_CTL_DEL:
    return "DEL";
  case EPOLL_CTL_MOD:
    return "MOD";
  default:
    return "UNKNOWN";
  }
}
