#include "muduo/net/ZlibStream.h"

#include <cassert>
#include <span>

namespace muduo::net {

namespace {

static_assert(sizeof(std::byte) == sizeof(Bytef),
              "std::byte and zlib Bytef must have identical size");

[[nodiscard]] Bytef *asBytef(std::byte *ptr) {
  return reinterpret_cast<Bytef *>(ptr);
}

[[nodiscard]] Bytef *asBytef(const std::byte *ptr) {
  // zlib's historical C API takes Bytef* for next_in, but input bytes are
  // treated as read-only by inflate/deflate.
  return reinterpret_cast<Bytef *>(const_cast<std::byte *>(ptr));
}

[[nodiscard]] Bytef *asBytef(const char *ptr) {
  return reinterpret_cast<Bytef *>(const_cast<char *>(ptr));
}

} // namespace

ZlibInputStream::ZlibInputStream(Buffer *output) : output_(output) {
  assert(output_ != nullptr);
  zerror_ = ::inflateInit(&zstream_);
}

ZlibInputStream::~ZlibInputStream() { (void)finish(); }

bool ZlibInputStream::write(StringPiece buf) {
  if (finished_ || zerror_ != Z_OK) {
    return false;
  }

  zstream_.next_in = asBytef(buf.data());
  zstream_.avail_in = static_cast<uInt>(buf.size());

  while (zstream_.avail_in > 0 && zerror_ == Z_OK) {
    zerror_ = decompress(Z_NO_FLUSH);
  }

  if (zerror_ == Z_STREAM_END) {
    finished_ = true;
    zerror_ = Z_OK;
  }
  return zerror_ == Z_OK;
}

bool ZlibInputStream::write(Buffer *input) {
  assert(input != nullptr);
  if (finished_ || zerror_ != Z_OK) {
    return false;
  }

  auto in = input->readableSpan();
  zstream_.next_in = asBytef(in.data());
  zstream_.avail_in = static_cast<uInt>(in.size());

  while (zstream_.avail_in > 0 && zerror_ == Z_OK) {
    zerror_ = decompress(Z_NO_FLUSH);
  }
  input->retrieve(in.size() - zstream_.avail_in);

  if (zerror_ == Z_STREAM_END) {
    finished_ = true;
    zerror_ = Z_OK;
  }
  return zerror_ == Z_OK;
}

bool ZlibInputStream::finish() {
  if (finished_) {
    return true;
  }
  if (zerror_ != Z_OK && zerror_ != Z_STREAM_END) {
    return false;
  }

  while (zerror_ == Z_OK) {
    zerror_ = decompress(Z_FINISH);
  }

  const bool ok = (zerror_ == Z_STREAM_END || zerror_ == Z_BUF_ERROR);
  const int endError = ::inflateEnd(&zstream_);
  finished_ = true;
  zerror_ = (ok && endError == Z_OK) ? Z_STREAM_END : endError;
  return ok && endError == Z_OK;
}

int ZlibInputStream::decompress(int flush) {
  output_->ensureWritableBytes(static_cast<size_t>(bufferSize_));
  auto out = output_->writableSpan();
  zstream_.next_out = asBytef(out.data());
  zstream_.avail_out = static_cast<uInt>(out.size());

  const int ret = ::inflate(&zstream_, flush);
  output_->hasWritten(out.size() - zstream_.avail_out);

  if (output_->writableBytes() == 0 && bufferSize_ < 65'536) {
    bufferSize_ *= 2;
  }
  return ret;
}

ZlibOutputStream::ZlibOutputStream(Buffer *output) : output_(output) {
  assert(output_ != nullptr);
  zerror_ = ::deflateInit(&zstream_, Z_DEFAULT_COMPRESSION);
}

ZlibOutputStream::~ZlibOutputStream() { (void)finish(); }

bool ZlibOutputStream::write(StringPiece buf) {
  if (finished_ || zerror_ != Z_OK) {
    return false;
  }

  zstream_.next_in = asBytef(buf.data());
  zstream_.avail_in = static_cast<uInt>(buf.size());
  while (zstream_.avail_in > 0 && zerror_ == Z_OK) {
    zerror_ = compress(Z_NO_FLUSH);
  }
  if (zstream_.avail_in == 0) {
    zstream_.next_in = nullptr;
  }
  return zerror_ == Z_OK;
}

bool ZlibOutputStream::write(Buffer *input) {
  assert(input != nullptr);
  if (finished_ || zerror_ != Z_OK) {
    return false;
  }

  auto in = input->readableSpan();
  zstream_.next_in = asBytef(in.data());
  zstream_.avail_in = static_cast<uInt>(in.size());
  while (zstream_.avail_in > 0 && zerror_ == Z_OK) {
    zerror_ = compress(Z_NO_FLUSH);
  }
  input->retrieve(in.size() - zstream_.avail_in);
  return zerror_ == Z_OK;
}

bool ZlibOutputStream::finish() {
  if (finished_) {
    return true;
  }
  if (zerror_ != Z_OK) {
    return false;
  }

  while (zerror_ == Z_OK) {
    zerror_ = compress(Z_FINISH);
  }
  const int endError = ::deflateEnd(&zstream_);
  const bool ok = (zerror_ == Z_STREAM_END) && (endError == Z_OK);
  finished_ = true;
  zerror_ = ok ? Z_STREAM_END : (endError != Z_OK ? endError : zerror_);
  return ok;
}

int ZlibOutputStream::compress(int flush) {
  output_->ensureWritableBytes(static_cast<size_t>(bufferSize_));
  auto out = output_->writableSpan();
  zstream_.next_out = asBytef(out.data());
  zstream_.avail_out = static_cast<uInt>(out.size());

  const int error = ::deflate(&zstream_, flush);
  output_->hasWritten(out.size() - zstream_.avail_out);
  if (output_->writableBytes() == 0 && bufferSize_ < 65'536) {
    bufferSize_ *= 2;
  }
  return error;
}

} // namespace muduo::net
