#pragma once

#include "muduo/base/noncopyable.h"
#if MUDUO_ENABLE_LEGACY_COMPAT
#include "muduo/base/StringPiece.h"
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace muduo::FileUtil {

namespace detail {

inline int statFd(int fd, struct stat *statbuf) { return ::fstat(fd, statbuf); }

inline ssize_t readFd(int fd, std::span<char> buffer) {
  return ::read(fd, buffer.data(), buffer.size());
}

} // namespace detail

template <typename String>
concept StringLike = requires(String &s, const char *p, size_t n) {
  { s.clear() } -> std::same_as<void>;
  s.reserve(n);
  s.append(p, n);
  { s.size() } -> std::convertible_to<size_t>;
};

class ReadSmallFile : noncopyable {
public:
  static constexpr int kBufferSize = 64 * 1024;

  explicit ReadSmallFile(std::string_view filename);
#if MUDUO_ENABLE_LEGACY_COMPAT
  explicit ReadSmallFile(StringPiece filename);
  explicit ReadSmallFile(StringArg filename);
#endif
  explicit ReadSmallFile(const std::filesystem::path &filename);
  ~ReadSmallFile();

  template <StringLike String>
  int readToString(int maxSize, String *content, int64_t *fileSize = nullptr,
                   int64_t *modifyTime = nullptr,
                   int64_t *createTime = nullptr);

  int readToBuffer(int *size);

  [[nodiscard]] const char *buffer() const { return buf_.data(); }
  [[nodiscard]] std::array<char, kBufferSize> &buffer() { return buf_; }

private:
  int fd_ = -1;
  int err_ = 0;
  std::array<char, kBufferSize> buf_;
};

template <StringLike String>
int readFile(std::string_view filename, int maxSize, String *content,
             int64_t *fileSize = nullptr, int64_t *modifyTime = nullptr,
             int64_t *createTime = nullptr) {
  ReadSmallFile file(filename);
  return file.readToString(maxSize, content, fileSize, modifyTime, createTime);
}

template <StringLike String>
int readFile(const std::string &filename, int maxSize, String *content,
             int64_t *fileSize = nullptr, int64_t *modifyTime = nullptr,
             int64_t *createTime = nullptr) {
  return readFile(std::string_view(filename), maxSize, content, fileSize,
                  modifyTime, createTime);
}

#if MUDUO_ENABLE_LEGACY_COMPAT
template <StringLike String>
int readFile(StringPiece filename, int maxSize, String *content,
             int64_t *fileSize = nullptr, int64_t *modifyTime = nullptr,
             int64_t *createTime = nullptr) {
  return readFile(filename.as_string_view(), maxSize, content, fileSize,
                  modifyTime, createTime);
}

template <StringLike String>
int readFile(StringArg filename, int maxSize, String *content,
             int64_t *fileSize = nullptr, int64_t *modifyTime = nullptr,
             int64_t *createTime = nullptr) {
  return readFile(filename.as_string_view(), maxSize, content, fileSize,
                  modifyTime, createTime);
}

template <StringLike String>
int readFile(const char *filename, int maxSize, String *content,
             int64_t *fileSize = nullptr, int64_t *modifyTime = nullptr,
             int64_t *createTime = nullptr) {
  return readFile(std::string_view(filename), maxSize, content, fileSize,
                  modifyTime, createTime);
}
#endif

template <StringLike String>
int readFile(const std::filesystem::path &filename, int maxSize,
             String *content, int64_t *fileSize = nullptr,
             int64_t *modifyTime = nullptr, int64_t *createTime = nullptr) {
  ReadSmallFile file(filename);
  return file.readToString(maxSize, content, fileSize, modifyTime, createTime);
}

class AppendFile : noncopyable {
public:
  explicit AppendFile(std::string_view filename);
#if MUDUO_ENABLE_LEGACY_COMPAT
  explicit AppendFile(StringPiece filename);
  explicit AppendFile(StringArg filename);
#endif
  explicit AppendFile(const std::filesystem::path &filename);
  ~AppendFile();

#if MUDUO_ENABLE_LEGACY_COMPAT
  void append(const char *logline, size_t len);
  void append(StringPiece logline);
  void append(StringArg logline);
#endif
  void append(std::string_view logline);
  void append(std::span<const char> logline);
  void flush();

  [[nodiscard]] off_t writtenBytes() const { return writtenBytes_; }

private:
  void appendBytes(std::span<const char> bytes);
  void reportIoError(std::string_view where);

  int fd_ = -1;
  off_t writtenBytes_ = 0;
};

} // namespace muduo::FileUtil

template <muduo::FileUtil::StringLike String>
int muduo::FileUtil::ReadSmallFile::readToString(int maxSize, String *content,
                                                 int64_t *fileSize,
                                                 int64_t *modifyTime,
                                                 int64_t *createTime) {
  static_assert(sizeof(off_t) == 8, "_FILE_OFFSET_BITS = 64");
  int err = err_;
  if (fd_ >= 0 && content != nullptr) {
    content->clear();

    if (fileSize != nullptr) {
      struct stat statbuf{};
      if (detail::statFd(fd_, &statbuf) == 0) {
        if (S_ISREG(statbuf.st_mode)) {
          *fileSize = statbuf.st_size;
          content->reserve(
              static_cast<size_t>(std::min<int64_t>(maxSize, *fileSize)));
        } else if (S_ISDIR(statbuf.st_mode)) {
          err = EISDIR;
        }
        if (modifyTime != nullptr) {
          *modifyTime = statbuf.st_mtime;
        }
        if (createTime != nullptr) {
          *createTime = statbuf.st_ctime;
        }
      } else {
        err = errno;
      }
    }

    while (content->size() < static_cast<size_t>(maxSize)) {
      const size_t toRead = std::min(
          static_cast<size_t>(maxSize) - content->size(), sizeof(buf_));
      auto chunk = std::span<char>{buf_.data(), toRead};
      const ssize_t n = detail::readFd(fd_, chunk);
      if (n > 0) {
        content->append(buf_.data(), static_cast<size_t>(n));
      } else {
        if (n < 0) {
          err = errno;
        }
        break;
      }
    }
  }
  return err;
}
