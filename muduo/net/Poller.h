#pragma once

#include "muduo/base/Timestamp.h"
#include "muduo/base/noncopyable.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace muduo::net {

class Channel;
class EventLoop;

class Poller : muduo::noncopyable {
public:
  using ChannelList = std::vector<Channel *>;

  explicit Poller(EventLoop *loop);
  virtual ~Poller();

  [[nodiscard]] virtual Timestamp poll(int timeoutMs,
                                       ChannelList *activeChannels) = 0;
  virtual void updateChannel(Channel *channel) = 0;
  virtual void removeChannel(Channel *channel) = 0;

  [[nodiscard]] virtual bool hasChannel(Channel *channel) const;

  [[nodiscard]] static std::unique_ptr<Poller> newDefaultPoller(EventLoop *loop);

  void assertInLoopThread() const;

protected:
  using ChannelMap = std::unordered_map<int, Channel *>;
  ChannelMap channels_;

private:
  EventLoop *ownerLoop_;
};

} // namespace muduo::net
