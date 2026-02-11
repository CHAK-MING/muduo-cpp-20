#include "muduo/base/FileUtil.h"

#include "muduo/base/Logging.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;

FileUtil::AppendFile::AppendFile(std::string_view filename)
    : fp_(::fopen(std::string(filename).c_str(), "ae")) {
  if (fp_ != nullptr) {
    ::setbuffer(fp_, buffer_.data(), buffer_.size());
  }
}

FileUtil::AppendFile::AppendFile(StringPiece filename)
    : AppendFile(filename.as_string_view()) {}

FileUtil::AppendFile::AppendFile(StringArg filename)
    : AppendFile(filename.as_string_view()) {}

FileUtil::AppendFile::AppendFile(const std::filesystem::path &filename)
    : fp_(::fopen(filename.c_str(), "ae")) {
  if (fp_ != nullptr) {
    ::setbuffer(fp_, buffer_.data(), buffer_.size());
  }
}

FileUtil::AppendFile::~AppendFile() {
  if (fp_ != nullptr) {
    ::fclose(fp_);
  }
}

void FileUtil::AppendFile::append(const char *logline, const size_t len) {
  if (fp_ == nullptr) {
    return;
  }

  size_t written = 0;
  while (written < len) {
    const size_t n = write(logline + written, len - written);
    if (n == 0) {
      const int err = ferror(fp_);
      if (err != 0) {
        const auto msg =
            std::format("AppendFile::append() failed {}\n", strerror_tl(err));
        const auto writtenErr = std::fwrite(msg.data(), 1, msg.size(), stderr);
        (void)writtenErr;
      }
      break;
    }
    written += n;
  }

  writtenBytes_ += static_cast<off_t>(written);
}

void FileUtil::AppendFile::append(std::string_view logline) {
  append(logline.data(), logline.size());
}

void FileUtil::AppendFile::append(StringPiece logline) {
  append(logline.as_string_view());
}

void FileUtil::AppendFile::append(StringArg logline) {
  append(logline.as_string_view());
}

void FileUtil::AppendFile::append(std::span<const char> logline) {
  append(logline.data(), logline.size());
}

void FileUtil::AppendFile::flush() {
  if (fp_ != nullptr) {
    ::fflush(fp_);
  }
}

size_t FileUtil::AppendFile::write(const char *logline, size_t len) {
  return ::fwrite_unlocked(logline, 1, len, fp_);
}

FileUtil::ReadSmallFile::ReadSmallFile(std::string_view filename)
    : fd_(::open(std::string(filename).c_str(), O_RDONLY | O_CLOEXEC)) {
  if (fd_ < 0) {
    err_ = errno;
  }
  buf_[0] = '\0';
}

FileUtil::ReadSmallFile::ReadSmallFile(StringPiece filename)
    : ReadSmallFile(filename.as_string_view()) {}

FileUtil::ReadSmallFile::ReadSmallFile(StringArg filename)
    : ReadSmallFile(filename.as_string_view()) {}

FileUtil::ReadSmallFile::ReadSmallFile(const std::filesystem::path &filename)
    : fd_(::open(filename.c_str(), O_RDONLY | O_CLOEXEC)) {
  if (fd_ < 0) {
    err_ = errno;
  }
  buf_[0] = '\0';
}

FileUtil::ReadSmallFile::~ReadSmallFile() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

int FileUtil::ReadSmallFile::readToBuffer(int *size) {
  int err = err_;
  if (fd_ >= 0) {
    const ssize_t n = ::pread(fd_, buf_.data(), buf_.size() - 1, 0);
    if (n >= 0) {
      if (size != nullptr) {
        *size = static_cast<int>(n);
      }
      buf_[static_cast<size_t>(n)] = '\0';
    } else {
      err = errno;
    }
  }
  return err;
}
