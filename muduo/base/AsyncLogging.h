#pragma once

#include "muduo/base/LogStream.h"
#include "muduo/base/Types.h"

#include <atomic>
#include <latch>
#include <memory>
#include <mutex>
#include <semaphore>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

namespace muduo {

class AsyncLogging {
public:
  AsyncLogging(string basename, std::int64_t rollSize, int flushInterval = 3,
               size_t shardCount = 0);
  ~AsyncLogging();

  AsyncLogging(const AsyncLogging &) = delete;
  AsyncLogging &operator=(const AsyncLogging &) = delete;

  void append(const char *logline, int len);

  void start();
  void stop();
  [[nodiscard]] size_t shardCount() const noexcept { return shardCount_; }

private:
  void threadFunc(std::stop_token stopToken);
  [[nodiscard]] size_t shardIndex() const;

  using Buffer = muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer>;
  using BufferVector = std::vector<std::unique_ptr<Buffer>>;
  using BufferPtr = BufferVector::value_type;
  [[nodiscard]] BufferPtr acquireBuffer();
  void recycleBuffer(BufferPtr buffer);

  struct Shard {
    std::mutex mutex;
    BufferPtr currentBuffer;
    BufferPtr nextBuffer;
    BufferVector buffers;
  };
  const int flushInterval_;
  std::atomic<bool> running_{false};
  const string basename_;
  const std::int64_t rollSize_;
  std::jthread thread_;
  std::latch latch_{1};
  size_t shardCount_ = 0;
  size_t shardMask_ = 0;
  std::vector<Shard> shards_;
  std::binary_semaphore wakeup_{0};
  std::mutex poolMutex_;
  BufferVector globalPool_;
  static constexpr size_t kGlobalPoolMax = 64;
};

} // namespace muduo
