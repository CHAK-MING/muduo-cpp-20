#include "muduo/base/AsyncLogging.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/LogFile.h"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>

using namespace muduo;

AsyncLogging::AsyncLogging(string basename, std::int64_t rollSize,
                           int flushInterval)
    : flushInterval_(flushInterval), basename_(std::move(basename)),
      rollSize_(rollSize) {
  for (auto &shard : shards_) {
    shard.currentBuffer = std::make_unique<Buffer>();
    shard.nextBuffer = std::make_unique<Buffer>();
    shard.buffers.reserve(8);
  }
  globalPool_.reserve(kGlobalPoolMax);
  for (size_t i = 0; i < 16; ++i) {
    globalPool_.emplace_back(std::make_unique<Buffer>());
  }
}

AsyncLogging::BufferPtr AsyncLogging::acquireBuffer() {
  std::lock_guard<std::mutex> lock(poolMutex_);
  if (!globalPool_.empty()) {
    BufferPtr buf = std::move(globalPool_.back());
    globalPool_.pop_back();
    buf->reset();
    return buf;
  }
  return std::make_unique<Buffer>();
}

void AsyncLogging::recycleBuffer(BufferPtr buffer) {
  if (!buffer) {
    return;
  }
  buffer->reset();
  std::lock_guard<std::mutex> lock(poolMutex_);
  if (globalPool_.size() < kGlobalPoolMax) {
    globalPool_.emplace_back(std::move(buffer));
  }
}

AsyncLogging::~AsyncLogging() {
  if (running_.load(std::memory_order_acquire)) {
    stop();
  }
}

size_t AsyncLogging::shardIndex() const {
  thread_local size_t cached =
      static_cast<size_t>(muduo::CurrentThread::tid()) % kShardCount;
  return cached;
}

void AsyncLogging::append(const char *logline, int len) {
  auto &shard = shards_.at(shardIndex());
  bool wakeup = false;
  {
    std::lock_guard<std::mutex> lock(shard.mutex);
    if (shard.currentBuffer->avail() >= len) {
      shard.currentBuffer->append(logline, static_cast<size_t>(len));
      return;
    }

    shard.buffers.emplace_back(std::move(shard.currentBuffer));
    if (shard.nextBuffer) {
      shard.currentBuffer = std::move(shard.nextBuffer);
    } else {
      shard.currentBuffer = acquireBuffer();
    }
    shard.currentBuffer->append(logline, static_cast<size_t>(len));
    wakeup = true;
  }

  if (wakeup) {
    wakeup_.release();
  }
}

void AsyncLogging::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
    return;
  }

  thread_ = std::jthread([this](std::stop_token st) { threadFunc(st); });
  latch_.wait();
}

void AsyncLogging::stop() {
  running_.store(false, std::memory_order_release);
  wakeup_.release();
  if (thread_.joinable()) {
    thread_.request_stop();
    thread_.join();
  }
}

void AsyncLogging::threadFunc(std::stop_token stopToken) {
  LogFile output(basename_, rollSize_, false);
  BufferVector buffersToWrite;
  buffersToWrite.reserve(64);
  BufferVector spareBuffers;
  spareBuffers.reserve(32);

  // Initialize spare buffers to avoid allocation at startup
  for (int i = 0; i < 16; ++i) {
    spareBuffers.emplace_back(std::make_unique<Buffer>());
  }

  auto collect = [&]() {
    for (auto &shard : shards_) {
      std::lock_guard<std::mutex> lock(shard.mutex);

      // If current buffer has data, move it to shard.buffers
      if (shard.currentBuffer->length() > 0) {
        shard.buffers.emplace_back(std::move(shard.currentBuffer));

        if (shard.nextBuffer) {
          shard.currentBuffer = std::move(shard.nextBuffer);
        } else if (!spareBuffers.empty()) {
          shard.currentBuffer = std::move(spareBuffers.back());
          spareBuffers.pop_back();
          shard.currentBuffer->reset();
        } else {
          shard.currentBuffer = acquireBuffer();
        }
      }

      // Replenish nextBuffer if it's missing (Zero Allocation Principle)
      if (!shard.nextBuffer) {
        if (!spareBuffers.empty()) {
          shard.nextBuffer = std::move(spareBuffers.back());
          spareBuffers.pop_back();
          shard.nextBuffer->reset();
        } else {
          shard.nextBuffer = acquireBuffer();
        }
      }

      // Move accumulated buffers from shard to local write queue
      if (!shard.buffers.empty()) {
        buffersToWrite.insert(buffersToWrite.end(),
                              std::make_move_iterator(shard.buffers.begin()),
                              std::make_move_iterator(shard.buffers.end()));
        shard.buffers.clear();
      }
    }
  };

  latch_.count_down();

  while (!stopToken.stop_requested() ||
         running_.load(std::memory_order_acquire)) {
    wakeup_.try_acquire_for(std::chrono::seconds(flushInterval_));

    // Collect buffers from all shards
    collect();

    // If no data to write, check if we should exit
    if (buffersToWrite.empty()) {
      if (stopToken.stop_requested() &&
          !running_.load(std::memory_order_acquire)) {
        break;
      }
      continue;
    }

    if (buffersToWrite.size() > 25) {
      const auto nowSec = std::chrono::time_point_cast<std::chrono::seconds>(
          std::chrono::system_clock::now());

      char buf[256];
      constexpr std::string_view kPrefix = "Dropped log messages at ";
      std::memcpy(buf, kPrefix.data(), kPrefix.size());
      char *p = buf + kPrefix.size();

      // Format time YYYYMMDD HH:MM:SS
      const time_t t = std::chrono::system_clock::to_time_t(nowSec);
      struct tm tm;
      localtime_r(&t, &tm);
      p += std::strftime(p, 32, "%Y%m%d %H:%M:%S", &tm);

      constexpr std::string_view kMiddle = ", ";
      std::memcpy(p, kMiddle.data(), kMiddle.size());
      p += kMiddle.size();

      auto [ptr, ec] =
          std::to_chars(p, buf + sizeof(buf), buffersToWrite.size() - 2);
      if (ec == std::errc{}) {
        p = ptr;
      }

      constexpr std::string_view kSuffix = " larger buffers\n";
      std::memcpy(p, kSuffix.data(), kSuffix.size());
      p += kSuffix.size();

      std::string_view dropped(buf, static_cast<size_t>(p - buf));
      std::fwrite(dropped.data(), 1, dropped.size(), stderr);
      output.append(dropped);
      buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
    }

    for (const auto &buffer : buffersToWrite) {
      output.append(buffer->toStringView());
    }

    output.flush();

    if (spareBuffers.size() < 32) {
      const size_t needed = 32 - spareBuffers.size();
      const size_t available = buffersToWrite.size();
      const size_t toMove = std::min(needed, available);

      spareBuffers.insert(
          spareBuffers.end(), std::make_move_iterator(buffersToWrite.begin()),
          std::make_move_iterator(buffersToWrite.begin() + toMove));
    }
    buffersToWrite.clear();

    if (stopToken.stop_requested() &&
        !running_.load(std::memory_order_acquire)) {
      collect();
      if (buffersToWrite.empty()) {
        break;
      }
    }
  }

  output.flush();

  for (auto &buf : spareBuffers) {
    recycleBuffer(std::move(buf));
  }
  for (auto &buf : buffersToWrite) {
    recycleBuffer(std::move(buf));
  }
}
