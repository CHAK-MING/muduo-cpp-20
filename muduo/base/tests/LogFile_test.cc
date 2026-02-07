#include "muduo/base/LogFile.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <format>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

std::vector<std::filesystem::path> findLogFiles(std::string_view basename) {
  std::vector<std::filesystem::path> files;
  for (const auto &entry : std::filesystem::directory_iterator(".")) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto name = entry.path().filename().string();
    if (name.starts_with(std::string(basename)) && name.ends_with(".log")) {
      files.emplace_back(entry.path());
    }
  }
  return files;
}

} // namespace

TEST(LogFile, AppendCreatesLogFile) {
  const std::string basename = std::format("muduo_logfile_test_{}", ::getpid());
  for (const auto &path : findLogFiles(basename)) {
    std::filesystem::remove(path);
  }

  {
    muduo::LogFile file(basename, 1024 * 1024, false);
    file.append("abc\n", 4);
    file.append("def\n", 4);
    file.flush();
  }

  const auto files = findLogFiles(basename);
  ASSERT_FALSE(files.empty());
  EXPECT_GT(std::filesystem::file_size(files.front()), 0U);

  for (const auto &path : files) {
    std::filesystem::remove(path);
  }
}
