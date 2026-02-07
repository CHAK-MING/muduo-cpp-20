#include "muduo/base/ThreadPool.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/Exception.h"

#include <sys/prctl.h>

#include <cstdio>
#include <format>

using namespace muduo;

ThreadPool::ThreadPool(string nameArg) : name_(std::move(nameArg)) {}

ThreadPool::~ThreadPool() {
  if (isRunning()) {
    stop();
  }
}

void ThreadPool::start(int numThreads) {
  if (isRunning()) {
    return;
  }

  running_.store(true, std::memory_order_release);
  if (maxQueueSize_ > 0) {
    boundedQueue_ = std::make_unique<BoundedBlockingQueue<Task>>(
        static_cast<int>(maxQueueSize_));
  } else {
    queue_ = std::make_unique<BlockingQueue<Task>>();
  }

  threads_.reserve(static_cast<size_t>(numThreads));
  for (int i = 0; i < numThreads; ++i) {
    const string threadName = std::format("{}{}", name_, i + 1);
    threads_.emplace_back([this, threadName](std::stop_token st) {
      runInThread(st, threadName);
    });
  }

  if (numThreads == 0 && threadInitCallback_) {
    threadInitCallback_();
  }
}

void ThreadPool::stop() {
  if (!isRunning()) {
    return;
  }

  running_.store(false, std::memory_order_release);
  for (auto &thr : threads_) {
    thr.request_stop();
  }

  for (auto &thr : threads_) {
    if (thr.joinable()) {
      thr.join();
    }
  }

  threads_.clear();
  queue_.reset();
  boundedQueue_.reset();
}

size_t ThreadPool::queueSize() const {
  if (boundedQueue_) {
    return boundedQueue_->size();
  }
  if (queue_) {
    return queue_->size();
  }
  return 0;
}

void ThreadPool::put(Task &&task) {
  if (boundedQueue_) {
    boundedQueue_->put(std::move(task));
  } else if (queue_) {
    queue_->put(std::move(task));
  }
}

std::optional<ThreadPool::Task> ThreadPool::take(std::stop_token stopToken) {
  if (boundedQueue_) {
    return boundedQueue_->take(stopToken);
  }
  if (queue_) {
    return queue_->take(stopToken);
  }
  return std::nullopt;
}

void ThreadPool::runInThread(std::stop_token stopToken,
                             const string &threadName) {
  CurrentThread::setName(threadName.empty() ? "ThreadPool"
                                            : threadName.c_str());
  ::prctl(PR_SET_NAME, CurrentThread::name());

  try {
    if (threadInitCallback_) {
      threadInitCallback_();
    }

    while (!stopToken.stop_requested()) {
      auto task = take(stopToken);
      if (!task.has_value()) {
        break;
      }
      if (!(*task)) {
        if (!isRunning()) {
          break;
        }
        continue;
      }
      (*task)();
    }

    CurrentThread::setName("finished");
  } catch (const Exception &ex) {
    CurrentThread::setName("crashed");
    std::fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    std::fprintf(stderr, "reason: %s\n", ex.what());
    std::fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    std::abort();
  } catch (const std::exception &ex) {
    CurrentThread::setName("crashed");
    std::fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    std::fprintf(stderr, "reason: %s\n", ex.what());
    std::abort();
  } catch (...) {
    CurrentThread::setName("crashed");
    std::fprintf(stderr, "unknown exception caught in ThreadPool %s\n",
                 name_.c_str());
    throw;
  }
}
