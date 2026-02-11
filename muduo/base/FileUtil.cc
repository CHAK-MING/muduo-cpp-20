#include "muduo/base/FileUtil.h"

#include "muduo/base/Print.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <format>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;

namespace {

constexpr int kAppendOpenFlags = O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC;
constexpr int kReadOnlyOpenFlags = O_RDONLY | O_CLOEXEC;

[[nodiscard]] std::filesystem::path makePath(std::string_view filename) {
  return std::filesystem::path{filename};
}

[[nodiscard]] int openForAppend(const std::filesystem::path &path) {
  return ::open(path.c_str(), kAppendOpenFlags, 0644);
}

[[nodiscard]] int openReadOnly(const std::filesystem::path &path) {
  return ::open(path.c_str(), kReadOnlyOpenFlags);
}

[[nodiscard]] ssize_t writeBytes(int fd, std::span<const char> bytes) {
  return ::write(fd, bytes.data(), bytes.size());
}

[[nodiscard]] ssize_t readBytesAt(int fd, std::span<char> bytes, off_t offset) {
  return ::pread(fd, bytes.data(), bytes.size(), offset);
}

} // namespace

FileUtil::AppendFile::AppendFile(std::string_view filename)
    : AppendFile(makePath(filename)) {}

#if MUDUO_ENABLE_LEGACY_COMPAT
FileUtil::AppendFile::AppendFile(StringPiece filename)
    : AppendFile(filename.as_string_view()) {}

FileUtil::AppendFile::AppendFile(StringArg filename)
    : AppendFile(filename.as_string_view()) {}
#endif

FileUtil::AppendFile::AppendFile(const std::filesystem::path &filename)
    : fd_(openForAppend(filename)) {}

FileUtil::AppendFile::~AppendFile() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

void FileUtil::AppendFile::appendBytes(std::span<const char> bytes) {
  if (bytes.empty() || fd_ < 0) {
    return;
  }

  size_t offset = 0;
  while (offset < bytes.size()) {
    const auto chunk = bytes.subspan(offset);
    const ssize_t n = writeBytes(fd_, chunk);
    if (n > 0) {
      offset += static_cast<size_t>(n);
      continue;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    reportIoError("AppendFile::append()");
    return;
  }
  writtenBytes_ += static_cast<off_t>(bytes.size());
}

#if MUDUO_ENABLE_LEGACY_COMPAT
void FileUtil::AppendFile::append(const char *logline, const size_t len) {
  appendBytes(std::span<const char>(logline, len));
}

void FileUtil::AppendFile::append(StringPiece logline) {
  append(logline.as_string_view());
}

void FileUtil::AppendFile::append(StringArg logline) {
  append(logline.as_string_view());
}
#endif

void FileUtil::AppendFile::append(std::string_view logline) {
  appendBytes(std::span<const char>(logline.data(), logline.size()));
}

void FileUtil::AppendFile::append(std::span<const char> logline) {
  appendBytes(logline);
}

void FileUtil::AppendFile::flush() {
  // Direct fd writes are not user-space buffered.
}

void FileUtil::AppendFile::reportIoError(std::string_view where) {
  const int err = errno;
  if (err != 0) {
    muduo::io::eprintln("{} failed: {}", where,
                        std::error_code(err, std::generic_category()).message());
  } else {
    muduo::io::eprintln("{} failed", where);
  }
}

FileUtil::ReadSmallFile::ReadSmallFile(std::string_view filename)
    : ReadSmallFile(makePath(filename)) {}

FileUtil::ReadSmallFile::ReadSmallFile(const std::filesystem::path &filename)
    : fd_(openReadOnly(filename)) {
  if (fd_ < 0) {
    err_ = errno;
  }
  buf_[0] = '\0';
}

#if MUDUO_ENABLE_LEGACY_COMPAT
FileUtil::ReadSmallFile::ReadSmallFile(StringPiece filename)
    : ReadSmallFile(filename.as_string_view()) {}

FileUtil::ReadSmallFile::ReadSmallFile(StringArg filename)
    : ReadSmallFile(filename.as_string_view()) {}
#endif

FileUtil::ReadSmallFile::~ReadSmallFile() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

int FileUtil::ReadSmallFile::readToBuffer(int *size) {
  int err = err_;
  if (fd_ >= 0) {
    const auto out = std::span<char>{buf_.data(), buf_.size() - 1};
    const ssize_t n = readBytesAt(fd_, out, 0);
    if (n >= 0) {
      if (size != nullptr) {
        *size = static_cast<int>(n);
      }
      buf_.at(static_cast<size_t>(n)) = '\0';
    } else {
      err = errno;
    }
  }
  return err;
}
