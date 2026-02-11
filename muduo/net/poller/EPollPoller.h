#pragma once

#include "muduo/net/Poller.h"

#include <vector>

struct epoll_event;

namespace muduo::net {

class EPollPoller : public Poller {
public:
  explicit EPollPoller(EventLoop *loop);
  ~EPollPoller() override;

  [[nodiscard]] Timestamp poll(int timeoutMs,
                               ChannelList *activeChannels) override;
  void updateChannel(Channel *channel) override;
  void removeChannel(Channel *channel) override;

private:
  static constexpr int kInitEventListSize = 16;
  static constexpr int kNew = -1;
  static constexpr int kAdded = 1;
  static constexpr int kDeleted = 2;

  static const char *operationToString(int op);

  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
  void update(int operation, Channel *channel);

  using EventList = std::vector<struct epoll_event>;

  int epollfd_;
  EventList events_;
};

} // namespace muduo::net
