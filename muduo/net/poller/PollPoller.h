#pragma once

#include "muduo/net/Poller.h"

#include <vector>

struct pollfd;

namespace muduo::net {

class PollPoller : public Poller {
public:
  explicit PollPoller(EventLoop *loop);
  ~PollPoller() override;

  [[nodiscard]] Timestamp poll(int timeoutMs,
                               ChannelList *activeChannels) override;
  void updateChannel(Channel *channel) override;
  void removeChannel(Channel *channel) override;

private:
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

  using PollFdList = std::vector<struct pollfd>;
  PollFdList pollfds_;
};

} // namespace muduo::net
