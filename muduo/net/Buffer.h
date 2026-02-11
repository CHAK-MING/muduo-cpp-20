#pragma once

#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include "muduo/base/copyable.h"
#include "muduo/net/Endian.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace muduo::net {

class Buffer : public muduo::copyable {
public:
  static constexpr size_t kCheapPrepend = 8;
  static constexpr size_t kInitialSize = 1024;

  explicit Buffer(size_t initialSize = kInitialSize)
      : buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend),
        writerIndex_(kCheapPrepend) {
    assert(readableBytes() == 0);
    assert(writableBytes() == initialSize);
    assert(prependableBytes() == kCheapPrepend);
  }

  void swap(Buffer &rhs) noexcept {
    buffer_.swap(rhs.buffer_);
    std::swap(readerIndex_, rhs.readerIndex_);
    std::swap(writerIndex_, rhs.writerIndex_);
  }

  [[nodiscard]] size_t readableBytes() const {
    return writerIndex_ - readerIndex_;
  }
  [[nodiscard]] size_t writableBytes() const {
    return buffer_.size() - writerIndex_;
  }
  [[nodiscard]] size_t prependableBytes() const { return readerIndex_; }

  [[nodiscard]] std::span<const std::byte> readableSpan() const {
    return {begin() + readerIndex_, readableBytes()};
  }

  [[nodiscard]] std::span<std::byte> writableSpan() {
    return {begin() + writerIndex_, writableBytes()};
  }

  [[nodiscard]] const std::byte *peek() const { return readableSpan().data(); }
  [[nodiscard]] std::string_view readableChars() const {
    return std::string_view{peekAsChar(), readableBytes()};
  }
  [[nodiscard]] const char *peekAsChar() const {
    return bytesToChars(peek());
  }

  [[nodiscard]] const std::byte *findCRLF() const { return findCRLF(peek()); }
  [[nodiscard]] const char *findCRLFChars() const { return findCRLFChars(peekAsChar()); }

  [[nodiscard]] const std::byte *findCRLF(const std::byte *start) const {
    assert(peek() <= start);
    assert(start <= beginWrite());
    auto haystack = std::span{start, static_cast<size_t>(beginWrite() - start)};
    if (auto match = std::ranges::search(haystack, kCRLF);
        match.begin() == haystack.end()) {
      return nullptr;
    } else {
      return std::to_address(match.begin());
    }
  }
  [[nodiscard]] const char *findCRLFChars(const char *start) const {
    const auto *crlf = findCRLF(charsToBytes(start));
    return crlf == nullptr ? nullptr : bytesToChars(crlf);
  }

  [[nodiscard]] const std::byte *findEOL() const { return findEOL(peek()); }

  [[nodiscard]] const std::byte *findEOL(const std::byte *start) const {
    assert(peek() <= start);
    assert(start <= beginWrite());
    auto haystack = std::span{start, static_cast<size_t>(beginWrite() - start)};
    auto it = std::ranges::find(haystack, std::byte{'\n'});
    if (it == haystack.end()) {
      return nullptr;
    }
    return std::to_address(it);
  }

  void retrieve(size_t len) {
    assert(len <= readableBytes());
    if (len < readableBytes()) {
      readerIndex_ += len;
    } else {
      retrieveAll();
    }
  }

  void retrieveUntil(const std::byte *end) {
    assert(peek() <= end);
    assert(end <= beginWrite());
    retrieve(static_cast<size_t>(end - peek()));
  }
  void retrieveUntil(const char *end) { retrieveUntil(charsToBytes(end)); }

  void retrieveInt64() { retrieve(sizeof(int64_t)); }
  void retrieveInt32() { retrieve(sizeof(int32_t)); }
  void retrieveInt16() { retrieve(sizeof(int16_t)); }
  void retrieveInt8() { retrieve(sizeof(int8_t)); }

  void retrieveAll() {
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend;
  }

  [[nodiscard]] std::string retrieveAllAsString() {
    return retrieveAsString(readableBytes());
  }

  [[nodiscard]] std::string retrieveAsString(size_t len) {
    assert(len <= readableBytes());
    std::string result(peekAsChar(), len);
    retrieve(len);
    return result;
  }

  [[nodiscard]] StringPiece toStringPiece() const {
    return StringPiece{peekAsChar(), readableBytes()};
  }

  void append(StringPiece str) {
    append(std::as_bytes(std::span{str.data(), str.size()}));
  }

  void append(const string &str) { append(std::string_view{str}); }

  void append(std::string_view str) {
    append(std::as_bytes(std::span{str.data(), str.size()}));
  }

  void append(const char *str) { append(std::string_view{str}); }

  template <size_t N> void append(const char (&str)[N]) {
    append(std::string_view{str, N - 1});
  }

  void append(const void *data, size_t len) {
    append(std::as_bytes(std::span{static_cast<const char *>(data), len}));
  }

  void append(std::span<const std::byte> data) {
    ensureWritableBytes(data.size());
    std::ranges::copy(data, beginWrite());
    hasWritten(data.size());
  }

  void ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
      makeSpace(len);
    }
    assert(writableBytes() >= len);
  }

  [[nodiscard]] std::byte *beginWrite() { return begin() + writerIndex_; }
  [[nodiscard]] const std::byte *beginWrite() const {
    return begin() + writerIndex_;
  }

  void hasWritten(size_t len) {
    assert(len <= writableBytes());
    writerIndex_ += len;
  }

  void unwrite(size_t len) {
    assert(len <= readableBytes());
    writerIndex_ -= len;
  }

  void appendInt64(int64_t x) {
    const auto be64 = static_cast<int64_t>(
        sockets::hostToNetwork64(static_cast<uint64_t>(x)));
    append(std::as_bytes(std::span{&be64, 1}));
  }

  void appendInt32(int32_t x) {
    const auto be32 = static_cast<int32_t>(
        sockets::hostToNetwork32(static_cast<uint32_t>(x)));
    append(std::as_bytes(std::span{&be32, 1}));
  }

  void appendInt16(int16_t x) {
    const auto be16 = static_cast<int16_t>(
        sockets::hostToNetwork16(static_cast<uint16_t>(x)));
    append(std::as_bytes(std::span{&be16, 1}));
  }

  void appendInt8(int8_t x) { append(std::as_bytes(std::span{&x, 1})); }

  [[nodiscard]] int64_t readInt64() {
    const int64_t result = peekInt64();
    retrieveInt64();
    return result;
  }

  [[nodiscard]] int32_t readInt32() {
    const int32_t result = peekInt32();
    retrieveInt32();
    return result;
  }

  [[nodiscard]] int16_t readInt16() {
    const int16_t result = peekInt16();
    retrieveInt16();
    return result;
  }

  [[nodiscard]] int8_t readInt8() {
    const int8_t result = peekInt8();
    retrieveInt8();
    return result;
  }

  [[nodiscard]] int64_t peekInt64() const {
    assert(readableBytes() >= sizeof(int64_t));
    uint64_t be64 = 0;
    std::memcpy(&be64, peek(), sizeof(be64));
    return static_cast<int64_t>(sockets::networkToHost64(be64));
  }

  [[nodiscard]] int32_t peekInt32() const {
    assert(readableBytes() >= sizeof(int32_t));
    uint32_t be32 = 0;
    std::memcpy(&be32, peek(), sizeof(be32));
    return static_cast<int32_t>(sockets::networkToHost32(be32));
  }

  [[nodiscard]] int16_t peekInt16() const {
    assert(readableBytes() >= sizeof(int16_t));
    uint16_t be16 = 0;
    std::memcpy(&be16, peek(), sizeof(be16));
    return static_cast<int16_t>(sockets::networkToHost16(be16));
  }

  [[nodiscard]] int8_t peekInt8() const {
    assert(readableBytes() >= sizeof(int8_t));
    return static_cast<int8_t>(*peek());
  }

  void prependInt64(int64_t x) {
    const auto be64 = static_cast<int64_t>(
        sockets::hostToNetwork64(static_cast<uint64_t>(x)));
    prepend(std::as_bytes(std::span{&be64, 1}));
  }

  void prependInt32(int32_t x) {
    const auto be32 = static_cast<int32_t>(
        sockets::hostToNetwork32(static_cast<uint32_t>(x)));
    prepend(std::as_bytes(std::span{&be32, 1}));
  }

  void prependInt16(int16_t x) {
    const auto be16 = static_cast<int16_t>(
        sockets::hostToNetwork16(static_cast<uint16_t>(x)));
    prepend(std::as_bytes(std::span{&be16, 1}));
  }

  void prependInt8(int8_t x) { prepend(std::as_bytes(std::span{&x, 1})); }

  void prepend(const void *data, size_t len) {
    prepend(std::as_bytes(std::span{static_cast<const char *>(data), len}));
  }

  void prepend(std::span<const std::byte> data) {
    assert(data.size() <= prependableBytes());
    readerIndex_ -= data.size();
    std::ranges::copy(data, begin() + readerIndex_);
  }

  void shrink(size_t reserve) {
    Buffer other;
    other.ensureWritableBytes(readableBytes() + reserve);
    other.append(readableSpan());
    swap(other);
  }

  [[nodiscard]] size_t internalCapacity() const { return buffer_.capacity(); }

  [[nodiscard]] ssize_t readFd(int fd, int *savedErrno);

private:
  [[nodiscard]] static const char *bytesToChars(const std::byte *ptr) {
    return reinterpret_cast<const char *>(ptr);
  }
  [[nodiscard]] static const std::byte *charsToBytes(const char *ptr) {
    return reinterpret_cast<const std::byte *>(ptr);
  }

  [[nodiscard]] std::byte *begin() { return buffer_.data(); }
  [[nodiscard]] const std::byte *begin() const { return buffer_.data(); }

  void makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
      buffer_.resize(writerIndex_ + len);
      return;
    }

    assert(kCheapPrepend < readerIndex_);
    const size_t readable = readableBytes();
    std::ranges::copy(std::span{begin() + readerIndex_, readable},
                      begin() + kCheapPrepend);
    readerIndex_ = kCheapPrepend;
    writerIndex_ = readerIndex_ + readable;
    assert(readable == readableBytes());
  }

  std::vector<std::byte> buffer_;
  size_t readerIndex_;
  size_t writerIndex_;

  static constexpr std::array<std::byte, 2> kCRLF{std::byte{'\r'},
                                                  std::byte{'\n'}};
};

} // namespace muduo::net
