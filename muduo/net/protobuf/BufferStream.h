#pragma once

#include "muduo/base/Logging.h"
#include "muduo/net/Buffer.h"

#include <google/protobuf/io/zero_copy_stream.h>

namespace muduo::net {

class BufferOutputStream : public ::google::protobuf::io::ZeroCopyOutputStream {
public:
  explicit BufferOutputStream(Buffer *buf)
      : buffer_(muduo::CheckNotNull("BufferOutputStream::buffer", buf)),
        originalSize_(buffer_->readableBytes()) {}

  bool Next(void **data, int *size) override {
    buffer_->ensureWritableBytes(4096);
    *data = buffer_->beginWrite();
    *size = static_cast<int>(buffer_->writableBytes());
    buffer_->hasWritten(static_cast<size_t>(*size));
    return true;
  }

  void BackUp(int count) override { buffer_->unwrite(static_cast<size_t>(count)); }

  [[nodiscard]] int64_t ByteCount() const override {
    return static_cast<int64_t>(buffer_->readableBytes() - originalSize_);
  }

private:
  Buffer *buffer_;
  size_t originalSize_;
};

} // namespace muduo::net
