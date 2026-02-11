#include "muduo/net/EventLoopThread.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/net/EventLoop.h"

#include <gtest/gtest.h>

#include <atomic>

using muduo::net::EventLoop;
using muduo::net::EventLoopThread;

class EventLoopThreadTest : public ::testing::Test {};

TEST_F(EventLoopThreadTest, NeverStartDestructsCleanly) {
  EventLoopThread thr;
  SUCCEED();
}

TEST_F(EventLoopThreadTest, StartAndQuitByDestructor) {
  EventLoopThread thr;
  EventLoop *loop = thr.startLoop();
  ASSERT_NE(loop, nullptr);

  std::atomic<bool> ran{false};
  loop->runInLoop(EventLoop::Functor([&ran] { ran.store(true); }));
  muduo::CurrentThread::sleepUsec(200 * 1000);
  EXPECT_TRUE(ran.load());
}

TEST_F(EventLoopThreadTest, ExplicitQuitBeforeDestructor) {
  EventLoopThread thr;
  EventLoop *loop = thr.startLoop();
  ASSERT_NE(loop, nullptr);

  loop->runInLoop(EventLoop::Functor([loop] { loop->quit(); }));
  muduo::CurrentThread::sleepUsec(200 * 1000);
  SUCCEED();
}
