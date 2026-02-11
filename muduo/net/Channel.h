#pragma once

#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"
#include "muduo/base/noncopyable.h"

#include <concepts>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace muduo::net {

class EventLoop;

class Channel : muduo::noncopyable {
public:
  using EventCallback = CallbackFunction<void()>;
  using ReadEventCallback = CallbackFunction<void(Timestamp)>;

  Channel(EventLoop *loop, int fd);
  ~Channel();

  void handleEvent(Timestamp receiveTime);

  template <typename F>
    requires CallbackBindable<F, ReadEventCallback>
  void setReadCallback(F &&cb) {
    readCallback_ = ReadEventCallback(std::forward<F>(cb));
  }

  template <typename F>
    requires CallbackBindable<F, EventCallback>
  void setWriteCallback(F &&cb) {
    writeCallback_ = EventCallback(std::forward<F>(cb));
  }

  template <typename F>
    requires CallbackBindable<F, EventCallback>
  void setCloseCallback(F &&cb) {
    closeCallback_ = EventCallback(std::forward<F>(cb));
  }

  template <typename F>
    requires CallbackBindable<F, EventCallback>
  void setErrorCallback(F &&cb) {
    errorCallback_ = EventCallback(std::forward<F>(cb));
  }

  void tie(const std::shared_ptr<void> &);

  [[nodiscard]] int fd() const { return fd_; }
  [[nodiscard]] int events() const { return events_; }
  void setRevents(int revents) { revents_ = revents; }
  void set_revents(int revents) { setRevents(revents); }
  [[nodiscard]] bool isNoneEvent() const { return events_ == kNoneEvent; }

  void enableReading();
  void disableReading();
  void enableWriting();
  void disableWriting();
  void disableAll();

  [[nodiscard]] bool isWriting() const { return (events_ & kWriteEvent) != 0; }
  [[nodiscard]] bool isReading() const { return (events_ & kReadEvent) != 0; }

  [[nodiscard]] int index() const { return index_; }
  void setIndex(int idx) { index_ = idx; }
  void set_index(int idx) { setIndex(idx); }

  [[nodiscard]] std::string reventsToString() const;
  [[nodiscard]] std::string eventsToString() const;

  void doNotLogHup() { logHup_ = false; }

  [[nodiscard]] EventLoop *ownerLoop() const { return loop_; }
  void remove();

private:
  static std::string eventsToString(int fd, int events);

  void update();
  void handleEventWithGuard(Timestamp receiveTime);

  static constexpr int kNoneEvent = 0;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop *loop_;
  const int fd_;
  int events_;
  int revents_;
  int index_;
  bool logHup_;

  std::weak_ptr<void> tie_;
  bool tied_;
  bool eventHandling_;
  bool addedToLoop_;
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

} // namespace muduo::net
