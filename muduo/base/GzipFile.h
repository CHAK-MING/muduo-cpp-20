#pragma once

#include "muduo/base/StringPiece.h"
#include <muduo/base/noncopyable.h>

#include <zlib.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace muduo {

class GzipFile : noncopyable {
public:
  GzipFile() = default;
  explicit GzipFile(std::nullptr_t) {}

  GzipFile(GzipFile &&rhs) noexcept
      : file_(std::exchange(rhs.file_, nullptr)) {}

  GzipFile &operator=(GzipFile &&rhs) noexcept {
    if (this != &rhs) {
      close();
      file_ = std::exchange(rhs.file_, nullptr);
    }
    return *this;
  }

  ~GzipFile() { close(); }

  [[nodiscard]] bool valid() const noexcept { return file_ != nullptr; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] gzFile nativeHandle() const noexcept { return file_; }

  void close() noexcept {
    if (file_ != nullptr) {
      ::gzclose(file_);
      file_ = nullptr;
    }
  }

  void swap(GzipFile &rhs) noexcept { std::swap(file_, rhs.file_); }

#if ZLIB_VERNUM >= 0x1240
  [[nodiscard]] bool setBuffer(int size) {
    return file_ != nullptr && ::gzbuffer(file_, size) == 0;
  }
#endif

  [[nodiscard]] int read(void *buf, int len) {
    return file_ != nullptr ? ::gzread(file_, buf, len) : -1;
  }

  [[nodiscard]] int read(std::span<std::byte> bytes) {
    return read(bytes.data(), static_cast<int>(bytes.size()));
  }

  [[nodiscard]] int write(std::string_view text) {
    return file_ != nullptr ? ::gzwrite(file_, text.data(),
                                        static_cast<unsigned>(text.size()))
                            : -1;
  }

  [[nodiscard]] int write(StringPiece buf) {
    return write(buf.as_string_view());
  }
  [[nodiscard]] int write(StringArg buf) { return write(buf.as_string_view()); }

  [[nodiscard]] int write(std::span<const std::byte> bytes) {
    return file_ != nullptr ? ::gzwrite(file_, bytes.data(),
                                        static_cast<unsigned>(bytes.size()))
                            : -1;
  }

  [[nodiscard]] std::int64_t tell() const {
    return file_ != nullptr ? static_cast<std::int64_t>(::gztell(file_)) : -1;
  }

#if ZLIB_VERNUM >= 0x1240
  [[nodiscard]] std::int64_t offset() const {
    return file_ != nullptr ? static_cast<std::int64_t>(::gzoffset(file_)) : -1;
  }
#endif

  [[nodiscard]] static GzipFile openForRead(std::string_view filename) {
    return open(filename, "rbe");
  }
  [[nodiscard]] static GzipFile openForAppend(std::string_view filename) {
    return open(filename, "abe");
  }
  [[nodiscard]] static GzipFile
  openForWriteExclusive(std::string_view filename) {
    return open(filename, "wbxe");
  }
  [[nodiscard]] static GzipFile
  openForWriteTruncate(std::string_view filename) {
    return open(filename, "wbe");
  }

  [[nodiscard]] static GzipFile
  openForRead(const std::filesystem::path &filename) {
    const std::string path = filename.string();
    return open(path, "rbe");
  }
  [[nodiscard]] static GzipFile
  openForAppend(const std::filesystem::path &filename) {
    const std::string path = filename.string();
    return open(path, "abe");
  }
  [[nodiscard]] static GzipFile
  openForWriteExclusive(const std::filesystem::path &filename) {
    const std::string path = filename.string();
    return open(path, "wbxe");
  }
  [[nodiscard]] static GzipFile
  openForWriteTruncate(const std::filesystem::path &filename) {
    const std::string path = filename.string();
    return open(path, "wbe");
  }

private:
  explicit GzipFile(gzFile file) : file_(file) {}

  [[nodiscard]] static GzipFile open(std::string_view filename,
                                     const char *mode) {
    const std::string path(filename);
    return GzipFile(::gzopen(path.c_str(), mode));
  }

  gzFile file_ = nullptr;
};

} // namespace muduo
