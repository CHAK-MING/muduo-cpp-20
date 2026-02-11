#include "muduo/base/ThreadPool.h"

#include "muduo/base/CurrentThread.h"

#include <sys/prctl.h>

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

  threads_.clear();
  workers_.clear();
  queueSlots_.reset();
  nextWorker_.store(0, std::memory_order_release);
  queuedTasks_.store(0, std::memory_order_release);
  waitingProducers_.store(0, std::memory_order_release);
  running_.store(true, std::memory_order_release);
  started_.store(true, std::memory_order_release);
  if (maxQueueSize_ > 0) {
    queueSlots_ = std::make_unique<std::counting_semaphore<INT_MAX>>(
        static_cast<std::ptrdiff_t>(maxQueueSize_));
  }

  workers_.reserve(static_cast<size_t>(numThreads));
  for (int i = 0; i < numThreads; ++i) {
    workers_.emplace_back(std::make_unique<WorkerState>());
  }

  threads_.reserve(static_cast<size_t>(numThreads));
  for (int i = 0; i < numThreads; ++i) {
    const string threadName = std::format("{}{}", name_, i + 1);
    auto thread = std::make_unique<Thread>(
        [this, threadName, i] { runInThread(threadName, static_cast<size_t>(i)); },
        threadName);
    thread->start();
    threads_.emplace_back(std::move(thread));
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
  if (queueSlots_) {
    const int waiting = waitingProducers_.exchange(0, std::memory_order_acq_rel);
    if (waiting > 0) {
      queueSlots_->release(waiting);
    }
  }
  for (const auto &worker : workers_) {
    worker->signal.release();
  }
  for (auto &thread : threads_) {
    thread->join();
  }
  threads_.clear();
  workers_.clear();
  queueSlots_.reset();
  queuedTasks_.store(0, std::memory_order_release);
}

void ThreadPool::setMaxQueueSize(int maxSize) {
  if (isRunning()) {
    return;
  }
  maxQueueSize_ = maxSize > 0 ? static_cast<size_t>(maxSize) : 0;
}

size_t ThreadPool::queueSize() const {
  return queuedTasks_.load(std::memory_order_acquire);
}

void ThreadPool::run(Task f) {
  if (!f) {
    return;
  }
  if (!isRunning()) {
    if (!started_.load(std::memory_order_acquire)) {
      f();
    }
    return;
  }
  if (threads_.empty()) {
    f();
    return;
  }
  (void)enqueueTask(std::move(f));
}

bool ThreadPool::enqueueTask(Task &&task) {
  if (!task || !isRunning()) {
    return false;
  }
  if (queueSlots_) {
    waitingProducers_.fetch_add(1, std::memory_order_acq_rel);
    queueSlots_->acquire();
    waitingProducers_.fetch_sub(1, std::memory_order_acq_rel);
    if (!isRunning()) {
      queueSlots_->release();
      return false;
    }
  }
  enqueueSharded(std::move(task));
  return true;
}

void ThreadPool::enqueueSharded(Task &&task) {
  if (!task || workers_.empty()) {
    return;
  }

  const size_t index =
      nextWorker_.fetch_add(1, std::memory_order_relaxed) % workers_.size();
  const auto &worker = workers_.at(index);
  bool needsWake = false;
  {
    std::scoped_lock lock(worker->mutex);
    needsWake = worker->queue.empty();
    worker->queue.emplace_back(std::move(task));
  }
  queuedTasks_.fetch_add(1, std::memory_order_release);
  if (needsWake) {
    worker->signal.release();
  }
}

std::optional<ThreadPool::Task> ThreadPool::popSharded(size_t workerIndex) {
  if (workers_.empty()) {
    return std::nullopt;
  }

  auto popOwn = [this](size_t index) -> std::optional<Task> {
    const auto &worker = workers_.at(index);
    std::scoped_lock lock(worker->mutex);
    if (worker->queue.empty()) {
      return std::nullopt;
    }
    Task task(std::move(worker->queue.front()));
    worker->queue.pop_front();
    queuedTasks_.fetch_sub(1, std::memory_order_release);
    if (queueSlots_) {
      queueSlots_->release();
    }
    return task;
  };

  if (auto own = popOwn(workerIndex); own.has_value()) {
    return own;
  }
  if (queuedTasks_.load(std::memory_order_acquire) == 0) {
    return std::nullopt;
  }

  const size_t size = workers_.size();
  for (size_t i = 1; i < size; ++i) {
    const size_t index = (workerIndex + i) % size;
    auto &victim = workers_.at(index);
    std::unique_lock<std::mutex> lock(victim->mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
      continue;
    }
    if (victim->queue.empty()) {
      continue;
    }
    Task task(std::move(victim->queue.back()));
    victim->queue.pop_back();
    queuedTasks_.fetch_sub(1, std::memory_order_release);
    if (queueSlots_) {
      queueSlots_->release();
    }
    return task;
  }

  return std::nullopt;
}

void ThreadPool::runInThread(const string &threadName, size_t workerIndex) {
  CurrentThread::setName(threadName.empty() ? "ThreadPool"
                                            : threadName.c_str());
  const char *threadLabel = CurrentThread::name();
  ::prctl(PR_SET_NAME, threadLabel == nullptr ? "unknown" : threadLabel);

  if (threadInitCallback_) {
    threadInitCallback_();
  }

  while (isRunning()) {
    if (auto task = popSharded(workerIndex);
        task.has_value() && static_cast<bool>(*task)) {
      (*task)();
      continue;
    }
    workers_.at(workerIndex)->signal.acquire();
  }

  CurrentThread::setName("finished");
}
