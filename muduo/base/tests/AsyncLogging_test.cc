#include "muduo/base/AsyncLogging.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
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

std::string readFile(const std::filesystem::path &path) {
  std::ifstream ifs(path, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>(ifs),
                     std::istreambuf_iterator<char>()};
}

} // namespace

TEST(AsyncLogging, WritesMessagesToFile) {
  const std::string basename =
      std::format("muduo_asynclog_test_{}_{}", ::getpid(),
                  std::chrono::steady_clock::now().time_since_epoch().count());
  for (const auto &path : findLogFiles(basename)) {
    std::filesystem::remove(path);
  }

  muduo::AsyncLogging logger(basename, 1024 * 1024, 1);
  logger.start();
  logger.append("line one\n", 9);
  logger.append("line two\n", 9);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  logger.stop();

  const auto files = findLogFiles(basename);
  ASSERT_FALSE(files.empty());
  const std::string content = readFile(files.front());
  EXPECT_NE(content.find("line one"), std::string::npos);
  EXPECT_NE(content.find("line two"), std::string::npos);

  for (const auto &path : files) {
    std::filesystem::remove(path);
  }
}
