#include "muduo/base/Thread.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/Exception.h"

#include <sys/prctl.h>

#include <cassert>
#include <cstdio>
#include <format>
#include <utility>

using namespace muduo;

std::atomic<int> Thread::numCreated_{0};

Thread::~Thread() {
  if (thread_.joinable()) {
    thread_.request_stop();
  }
}

void Thread::setDefaultName() {
  const int num = ++numCreated_;
  if (name_.empty()) {
    name_ = std::format("Thread{}", num);
  }
}

void Thread::runInThread() {
  tid_.store(static_cast<pid_t>(CurrentThread::tid()),
             std::memory_order_release);
  tid_.notify_one();

  CurrentThread::setName(name_.empty() ? "muduoThread" : name_.c_str());
  ::prctl(PR_SET_NAME, CurrentThread::name());

  try {
    func_();
    CurrentThread::setName("finished");
  } catch (const Exception &ex) {
    CurrentThread::setName("crashed");
    std::fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
    std::fprintf(stderr, "reason: %s\n", ex.what());
    std::fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    std::abort();
  } catch (const std::exception &ex) {
    CurrentThread::setName("crashed");
    std::fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
    std::fprintf(stderr, "reason: %s\n", ex.what());
    std::abort();
  } catch (...) {
    CurrentThread::setName("crashed");
    std::fprintf(stderr, "unknown exception caught in Thread %s\n",
                 name_.c_str());
    throw;
  }
}

void Thread::start() {
  assert(!started_);
  started_ = true;

  thread_ = std::jthread([this](std::stop_token) { runInThread(); });
  while (tid_.load(std::memory_order_acquire) == 0) {
    tid_.wait(0, std::memory_order_relaxed);
  }
  assert(tid_.load(std::memory_order_acquire) > 0);
}

int Thread::join() {
  assert(thread_.joinable());
  if (thread_.joinable()) {
    thread_.join();
  }
  return 0;
}
