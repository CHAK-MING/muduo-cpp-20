#include "muduo/net/poller/PollPoller.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"

#include <cerrno>
#include <poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop *loop) : Poller(loop) {}

PollPoller::~PollPoller() = default;

Timestamp PollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  int numEvents =
      ::poll(pollfds_.empty() ? nullptr : pollfds_.data(), pollfds_.size(), timeoutMs);
  const int savedErrno = errno;
  const Timestamp now(Timestamp::now());

  if (numEvents > 0) {
    muduo::logTrace("{} events happened", numEvents);
    fillActiveChannels(numEvents, activeChannels);
  } else if (numEvents == 0) {
    muduo::logTrace("nothing happened");
  } else if (savedErrno != EINTR) {
    errno = savedErrno;
    muduo::logSysErr("PollPoller::poll()");
  }

  return now;
}

void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList *activeChannels) const {
  activeChannels->reserve(activeChannels->size() + static_cast<size_t>(numEvents));
  for (const auto &pfd : pollfds_) {
    if (numEvents <= 0) {
      break;
    }

    if (pfd.revents <= 0) {
      continue;
    }

    --numEvents;
    const int fd = pfd.fd >= 0 ? pfd.fd : -pfd.fd - 1;
    const auto it = channels_.find(fd);
    assert(it != channels_.end());

    auto *channel = it->second;
    channel->setRevents(pfd.revents);
    activeChannels->push_back(channel);
  }
}

void PollPoller::updateChannel(Channel *channel) {
  Poller::assertInLoopThread();
  muduo::logTrace("fd = {} events = {}", channel->fd(), channel->events());

  if (channel->index() < 0) {
    assert(!channels_.contains(channel->fd()));

    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    pollfds_.push_back(pfd);

    const int idx = static_cast<int>(pollfds_.size()) - 1;
    channel->setIndex(idx);
    channels_[pfd.fd] = channel;
    return;
  }

  assert(channels_.contains(channel->fd()));
  assert(channels_[channel->fd()] == channel);

  const int idx = channel->index();
  assert(idx >= 0 && static_cast<size_t>(idx) < pollfds_.size());

  auto &pfd = pollfds_[idx];
  assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd() - 1);
  pfd.fd = channel->fd();
  pfd.events = static_cast<short>(channel->events());
  pfd.revents = 0;

  if (channel->isNoneEvent()) {
    pfd.fd = -channel->fd() - 1;
  }
}

void PollPoller::removeChannel(Channel *channel) {
  Poller::assertInLoopThread();
  muduo::logTrace("fd = {}", channel->fd());

  assert(channels_.contains(channel->fd()));
  assert(channels_[channel->fd()] == channel);
  assert(channel->isNoneEvent());

  const int idx = channel->index();
  assert(idx >= 0 && static_cast<size_t>(idx) < pollfds_.size());

  const auto &pfd = pollfds_[idx];
  (void)pfd;
  assert((pfd.fd == -channel->fd() - 1 || pfd.fd == channel->fd()) &&
         pfd.events == channel->events());

  [[maybe_unused]] const auto erased = channels_.erase(channel->fd());
  assert(erased == 1);

  if (static_cast<size_t>(idx) == pollfds_.size() - 1) {
    pollfds_.pop_back();
    return;
  }

  int channelAtEnd = pollfds_.back().fd;
  std::iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);
  if (channelAtEnd < 0) {
    channelAtEnd = -channelAtEnd - 1;
  }
  channels_[channelAtEnd]->setIndex(idx);
  pollfds_.pop_back();
}
