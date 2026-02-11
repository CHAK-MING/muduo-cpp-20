#include "muduo/base/GzipFile.h"

#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

TEST(GzipFile, WriteReadAndExclusiveCreate) {
  using namespace std::string_view_literals;

  const std::filesystem::path path = "/tmp/gzipfile_test_cpp20.gz";
  std::error_code ec;
  std::filesystem::remove(path, ec);

  constexpr std::string_view kData =
      "123456789012345678901234567890123456789012345678901234567890\n";

  {
    auto writer = muduo::GzipFile::openForWriteTruncate(path);
    ASSERT_TRUE(writer.valid());
    EXPECT_GT(writer.write(kData), 0);
    EXPECT_GT(writer.write("sp\n"sv), 0);
    EXPECT_GT(writer.write("sa\n"sv), 0);
    EXPECT_GT(writer.tell(), 0);
  }

  {
    auto reader = muduo::GzipFile::openForRead(path);
    ASSERT_TRUE(reader.valid());

    std::array<char, 256> buf{};
    const int nr = reader.read(buf.data(), static_cast<int>(buf.size() - 1));
    ASSERT_GT(nr, 0);

    const std::string_view got(buf.data(), static_cast<size_t>(nr));
    EXPECT_EQ(got, std::string(kData) + "sp\nsa\n");
  }

  {
    errno = 0;
    auto writer = muduo::GzipFile::openForWriteExclusive(path);
    EXPECT_FALSE(writer.valid());
    EXPECT_EQ(errno, EEXIST);
  }

  std::filesystem::remove(path, ec);
}

TEST(GzipFile, AppendAndMoveSemantics) {
  const std::filesystem::path path = "/tmp/gzipfile_append_move_cpp20.gz";
  std::error_code ec;
  std::filesystem::remove(path, ec);

  {
    auto writer = muduo::GzipFile::openForWriteTruncate(path);
    ASSERT_TRUE(writer.valid());
    EXPECT_GT(writer.write("A"), 0);

    auto moved = std::move(writer);
    EXPECT_FALSE(writer.valid());
    ASSERT_TRUE(moved.valid());
    EXPECT_GT(moved.write("B"), 0);
  }

  {
    auto appender = muduo::GzipFile::openForAppend(path);
    ASSERT_TRUE(appender.valid());
    EXPECT_GT(appender.write("C"), 0);
  }

  {
    auto reader = muduo::GzipFile::openForRead(path);
    ASSERT_TRUE(reader.valid());

    std::array<char, 16> buf{};
    const int nr = reader.read(buf.data(), static_cast<int>(buf.size() - 1));
    ASSERT_EQ(nr, 3);
    EXPECT_EQ(std::string_view(buf.data(), static_cast<size_t>(nr)), "ABC");
  }

  std::filesystem::remove(path, ec);
}
