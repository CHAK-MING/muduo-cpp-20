#include "muduo/net/Timer.h"

#include <utility>

using namespace muduo::net;

std::atomic<std::int64_t> Timer::s_numCreated_{0};

void Timer::restart(Timestamp now) {
  if (repeat_) {
    expiration_ = addTime(now, interval_);
  } else {
    expiration_ = Timestamp::invalid();
  }
}
