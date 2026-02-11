#pragma once

#include "muduo/base/noncopyable.h"
#include "muduo/net/Buffer.h"
#if MUDUO_ENABLE_LEGACY_COMPAT
#include "muduo/base/StringPiece.h"
#endif

#include <cstdint>
#include <string>
#include <string_view>

#include <zlib.h>

namespace muduo::net {

// Input is zlib compressed data, output is uncompressed data.
class ZlibInputStream : muduo::noncopyable {
public:
  explicit ZlibInputStream(Buffer *output);
  ~ZlibInputStream();

  [[nodiscard]] bool write(std::string_view buf);
#if MUDUO_ENABLE_LEGACY_COMPAT
  [[nodiscard]] bool write(StringPiece buf);
  [[nodiscard]] bool write(const char *buf) { return write(std::string_view{buf}); }
#endif
  [[nodiscard]] bool write(const std::string &buf) {
    return write(std::string_view{buf});
  }
  [[nodiscard]] bool write(Buffer *input);
  [[nodiscard]] bool finish();

  [[nodiscard]] const char *zlibErrorMessage() const { return zstream_.msg; }
  [[nodiscard]] int zlibErrorCode() const { return zerror_; }
  [[nodiscard]] std::int64_t inputBytes() const { return zstream_.total_in; }
  [[nodiscard]] std::int64_t outputBytes() const { return zstream_.total_out; }
  [[nodiscard]] int internalOutputBufferSize() const { return bufferSize_; }

private:
  int decompress(int flush);

  Buffer *output_;
  z_stream zstream_{};
  int zerror_{Z_OK};
  int bufferSize_{1024};
  bool finished_{false};
};

// Input is uncompressed data, output is zlib compressed data.
class ZlibOutputStream : muduo::noncopyable {
public:
  explicit ZlibOutputStream(Buffer *output);
  ~ZlibOutputStream();

  [[nodiscard]] bool write(std::string_view buf);
#if MUDUO_ENABLE_LEGACY_COMPAT
  [[nodiscard]] bool write(StringPiece buf);
  [[nodiscard]] bool write(const char *buf) { return write(std::string_view{buf}); }
#endif
  [[nodiscard]] bool write(const std::string &buf) {
    return write(std::string_view{buf});
  }
  [[nodiscard]] bool write(Buffer *input);
  [[nodiscard]] bool finish();

  [[nodiscard]] const char *zlibErrorMessage() const { return zstream_.msg; }
  [[nodiscard]] int zlibErrorCode() const { return zerror_; }
  [[nodiscard]] std::int64_t inputBytes() const { return zstream_.total_in; }
  [[nodiscard]] std::int64_t outputBytes() const { return zstream_.total_out; }
  [[nodiscard]] int internalOutputBufferSize() const { return bufferSize_; }

private:
  int compress(int flush);

  Buffer *output_;
  z_stream zstream_{};
  int zerror_{Z_OK};
  int bufferSize_{1024};
  bool finished_{false};
};

} // namespace muduo::net
