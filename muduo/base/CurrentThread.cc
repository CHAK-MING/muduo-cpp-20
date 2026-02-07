#include "muduo/base/CurrentThread.h"

#include <boost/stacktrace.hpp>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <format>
#include <thread>

namespace {

pid_t getTid() { return gettid(); }

void afterFork() {
  muduo::CurrentThread::t_cachedTid = 0;
  muduo::CurrentThread::t_threadName = "main";
  muduo::CurrentThread::cacheTid();
}

class ThreadNameInitializer {
public:
  ThreadNameInitializer() {
    muduo::CurrentThread::t_threadName = "main";
    muduo::CurrentThread::cacheTid();
    ::pthread_atfork(nullptr, nullptr, &afterFork);
  }
};

ThreadNameInitializer g_threadNameInitializer;

} // namespace

namespace muduo::CurrentThread {

constinit thread_local int t_cachedTid = 0;
constinit thread_local std::array<char, 32> t_tidString{};
constinit thread_local int t_tidStringLength = 6;
constinit thread_local const char *t_threadName = "unknown";

void cacheTid() {
  if (t_cachedTid == 0) {
    t_cachedTid = getTid();
    auto out = std::format_to_n(t_tidString.data(), t_tidString.size(),
                                "{:5d} ", t_cachedTid);
    t_tidStringLength = static_cast<int>(
        std::min(static_cast<size_t>(out.size), t_tidString.size() - 1));
    t_tidString.at(static_cast<size_t>(t_tidStringLength)) = '\0';
  }
}

void setName(const char *name) { t_threadName = name; }

bool isMainThread() { return tid() == ::getpid(); }

void sleepUsec(int64_t usec) {
  std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

string stackTrace(bool demangle) {
  (void)demangle;
  return boost::stacktrace::to_string(boost::stacktrace::stacktrace());
}

} // namespace muduo::CurrentThread
