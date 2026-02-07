#include "muduo/base/Logging.h"

#include <gtest/gtest.h>

#include <mutex>
#include <string>

namespace {

std::mutex g_logMutex;
std::string g_logOutput;

void testOutput(const char* msg, int len) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  g_logOutput.append(msg, static_cast<size_t>(len));
}

void testFlush() {}

} // namespace

TEST(Logging, StreamAndFormatApis) {
  {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logOutput.clear();
  }

  muduo::Logger::setOutput(testOutput);
  muduo::Logger::setFlush(testFlush);
  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::INFO);

  muduo::logInfo() << "hello-stream";
  muduo::logInfo("hello-fmt {}", 42);

  std::string out;
  {
    std::lock_guard<std::mutex> lock(g_logMutex);
    out = g_logOutput;
  }
  EXPECT_NE(out.find("hello-stream"), std::string::npos);
  EXPECT_NE(out.find("hello-fmt 42"), std::string::npos);
}

TEST(Logging, LogLevelSuppressesInfo) {
  {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logOutput.clear();
  }

  muduo::Logger::setOutput(testOutput);
  muduo::Logger::setFlush(testFlush);
  muduo::Logger::setLogLevel(muduo::Logger::LogLevel::WARN);

  muduo::logInfo() << "should-not-appear";
  muduo::logInfo("fmt-should-not-appear {}", 1);
  muduo::logWarn() << "warn-appears";

  std::string out;
  {
    std::lock_guard<std::mutex> lock(g_logMutex);
    out = g_logOutput;
  }
  EXPECT_EQ(out.find("should-not-appear"), std::string::npos);
  EXPECT_EQ(out.find("fmt-should-not-appear"), std::string::npos);
  EXPECT_NE(out.find("warn-appears"), std::string::npos);
}
