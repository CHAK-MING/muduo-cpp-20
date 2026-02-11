#include "muduo/base/FileUtil.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <filesystem>
#include <string>

TEST(FileUtil, ReadFileBasicCases) {
  std::string content;
  int64_t size = 0;

  const int errCmdline =
      muduo::FileUtil::readFile("/proc/self/cmdline", 1024, &content, &size);
  EXPECT_EQ(errCmdline, 0);
  EXPECT_LE(content.size(), static_cast<size_t>(1024));

  const int errDir = muduo::FileUtil::readFile("/proc/self", 1024, &content, &size);
  EXPECT_EQ(errDir, EISDIR);

  const int errNotFound =
      muduo::FileUtil::readFile("/definitely/not/exist", 1024, &content, &size);
  EXPECT_NE(errNotFound, 0);

  const int errZero = muduo::FileUtil::readFile("/dev/zero", 256, &content, &size);
  EXPECT_EQ(errZero, 0);
  EXPECT_EQ(content.size(), static_cast<size_t>(256));
}

TEST(FileUtil, AppendFileWritesBytes) {
  const std::string path = "/tmp/muduo_fileutil_test.log";
  std::error_code ec;
  std::filesystem::remove(path, ec);
  muduo::FileUtil::AppendFile file{std::string_view(path)};

  file.append("abc", 3);
  file.append(std::string_view("def"));
  file.append(muduo::StringPiece("ghi"));
  file.append(muduo::StringArg("jkl"));
  file.flush();

  EXPECT_GE(file.writtenBytes(), 12);

  std::string content;
  int64_t size = 0;
  const int err = muduo::FileUtil::readFile(path, 1024, &content, &size);
  EXPECT_EQ(err, 0);
  EXPECT_GE(content.size(), static_cast<size_t>(12));
  std::filesystem::remove(path, ec);
}
