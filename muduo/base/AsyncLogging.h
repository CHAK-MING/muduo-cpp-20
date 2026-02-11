#pragma once

#include "muduo/base/LogStream.h"
#include "muduo/base/Types.h"
#include "muduo/base/noncopyable.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

namespace muduo {

class AsyncLogging : noncopyable {
public:
  AsyncLogging(string basename, std::int64_t rollSize, int flushInterval = 3,
               size_t shardCount = 0);
  ~AsyncLogging();

  void append(const char *logline, int len);

  void start();
  void stop();
  [[nodiscard]] bool started() const noexcept {
    return started_.load(std::memory_order_acquire);
  }
  [[nodiscard]] size_t shardCount() const noexcept { return shardCount_; }

private:
  static constexpr size_t kCacheLineSize = 64;

  void threadFunc(std::stop_token stopToken);
  [[nodiscard]] size_t shardIndex() const noexcept;

  using Buffer = muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer>;
  using BufferVector = std::vector<std::unique_ptr<Buffer>>;
  using BufferPtr = BufferVector::value_type;
  [[nodiscard]] BufferPtr acquireBuffer();
  void recycleBuffer(BufferPtr buffer);

  struct alignas(kCacheLineSize) Shard {
    std::mutex mutex;
    BufferPtr currentBuffer;
    BufferPtr nextBuffer;
    BufferVector buffers;
  };
  const int flushInterval_;
  std::atomic<bool> started_{false};
  const string basename_;
  const std::int64_t rollSize_;
  std::jthread thread_;
  std::binary_semaphore startedSignal_{0};
  size_t shardCount_ = 0;
  size_t shardMask_ = 0;
  std::vector<Shard> shards_;
  std::binary_semaphore wakeup_{0};
  std::mutex poolMutex_;
  BufferVector globalPool_;
  static constexpr size_t kGlobalPoolMax = 64;
};

} // namespace muduo
