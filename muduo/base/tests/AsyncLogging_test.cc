#include "muduo/base/AsyncLogging.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <latch>
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

size_t countOccurrences(const std::string &text, std::string_view token) {
  size_t count = 0;
  size_t pos = 0;
  while ((pos = text.find(token, pos)) != std::string::npos) {
    ++count;
    pos += token.size();
  }
  return count;
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

TEST(AsyncLogging, MultiThreadStressNoLoss) {
  using namespace std::chrono_literals;
  const std::string basename =
      std::format("muduo_asynclog_stress_{}_{}", ::getpid(),
                  std::chrono::steady_clock::now().time_since_epoch().count());
  for (const auto &path : findLogFiles(basename)) {
    std::filesystem::remove(path);
  }

  muduo::AsyncLogging logger(basename, 20 * 1024 * 1024, 1);
  logger.start();

  constexpr int kThreads = 10;
  constexpr int kPerThread = 2000;
  std::latch ready(kThreads);
  std::vector<std::jthread> workers;
  workers.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([&logger, &ready, t](std::stop_token) {
      ready.count_down();
      ready.wait();
      for (int i = 0; i < kPerThread; ++i) {
        const std::string line = std::format("stress-thread={} seq={}\n", t, i);
        logger.append(line.data(), static_cast<int>(line.size()));
      }
    });
  }

  for (auto &worker : workers) {
    worker.join();
  }

  std::this_thread::sleep_for(150ms);
  logger.stop();

  const auto files = findLogFiles(basename);
  ASSERT_FALSE(files.empty());

  std::string content;
  for (const auto &path : files) {
    content += readFile(path);
  }
  const size_t count = countOccurrences(content, "stress-thread=");
  EXPECT_EQ(count, static_cast<size_t>(kThreads * kPerThread));

  for (const auto &path : files) {
    std::filesystem::remove(path);
  }
}
