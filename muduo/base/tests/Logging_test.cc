#include "muduo/base/Logging.h"

#include <gtest/gtest.h>

#include <atomic>
#include <latch>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

std::mutex g_logMutex;
std::string g_logOutput;

void testOutput(const char* msg, int len) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  g_logOutput.append(msg, static_cast<size_t>(len));
}

void testFlush() {}

void clearLogOutput() {
  std::lock_guard<std::mutex> lock(g_logMutex);
  g_logOutput.clear();
}

std::string snapshotLogOutput() {
  std::lock_guard<std::mutex> lock(g_logMutex);
  return g_logOutput;
}

size_t countOccurrences(const std::string& s, std::string_view token) {
  size_t count = 0;
  size_t pos = 0;
  while ((pos = s.find(token, pos)) != std::string::npos) {
    ++count;
    pos += token.size();
  }
  return count;
}

} // namespace

TEST(Logging, StreamAndFormatApis) {
  clearLogOutput();

  muduo::Logger::setOutput(testOutput);
  muduo::Logger::setFlush(testFlush);
  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::INFO);

  muduo::logInfo() << "hello-stream";
  muduo::logInfo("hello-fmt {}", 42);

  const std::string out = snapshotLogOutput();
  EXPECT_NE(out.find("hello-stream"), std::string::npos);
  EXPECT_NE(out.find("hello-fmt 42"), std::string::npos);
}

TEST(Logging, LogLevelSuppressesInfo) {
  clearLogOutput();

  muduo::Logger::setOutput(testOutput);
  muduo::Logger::setFlush(testFlush);
  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::WARN);

  muduo::logInfo() << "should-not-appear";
  muduo::logInfo("fmt-should-not-appear {}", 1);
  muduo::logWarn() << "warn-appears";

  const std::string out = snapshotLogOutput();
  EXPECT_EQ(out.find("should-not-appear"), std::string::npos);
  EXPECT_EQ(out.find("fmt-should-not-appear"), std::string::npos);
  EXPECT_NE(out.find("warn-appears"), std::string::npos);
}

TEST(Logging, LogLevelCoverageAcrossTraceToError) {
  clearLogOutput();

  muduo::Logger::setOutput(testOutput);
  muduo::Logger::setFlush(testFlush);
  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::INFO);

  muduo::logTrace() << "trace-hidden";
  muduo::logDebug() << "debug-hidden";
  muduo::logInfo() << "info-visible";
  muduo::logWarn() << "warn-visible";
  muduo::logError() << "error-visible";

  std::string out = snapshotLogOutput();
  EXPECT_EQ(out.find("trace-hidden"), std::string::npos);
  EXPECT_EQ(out.find("debug-hidden"), std::string::npos);
  EXPECT_NE(out.find("info-visible"), std::string::npos);
  EXPECT_NE(out.find("warn-visible"), std::string::npos);
  EXPECT_NE(out.find("error-visible"), std::string::npos);

  clearLogOutput();
  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::TRACE);
  muduo::logTrace() << "trace-visible";
  muduo::logDebug() << "debug-visible";
  out = snapshotLogOutput();
  EXPECT_NE(out.find("trace-visible"), std::string::npos);
  EXPECT_NE(out.find("debug-visible"), std::string::npos);
}

TEST(Logging, MultiThreadStressNoLoss) {
  clearLogOutput();

  muduo::Logger::setOutput(testOutput);
  muduo::Logger::setFlush(testFlush);
  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::INFO);

  constexpr int kThreads = 8;
  constexpr int kPerThread = 800;
  std::latch ready(kThreads);
  std::vector<std::jthread> workers;
  workers.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([t, &ready](std::stop_token) {
      ready.count_down();
      ready.wait();
      for (int i = 0; i < kPerThread; ++i) {
        muduo::logInfo("mt-thread={} seq={}", t, i);
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  const std::string out = snapshotLogOutput();
  const size_t count = countOccurrences(out, "mt-thread=");
  EXPECT_EQ(count, static_cast<size_t>(kThreads * kPerThread));
}
