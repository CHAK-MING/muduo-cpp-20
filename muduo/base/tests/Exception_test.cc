#include "muduo/base/Exception.h"

#include <gtest/gtest.h>
#include <source_location>

TEST(Exception, StoresMessageAndStack) {
  muduo::Exception ex("oops");
  EXPECT_STREQ(ex.what(), "oops");
  EXPECT_EQ(ex.whatView(), "oops");
  EXPECT_FALSE(ex.stackTraceView().empty());
}

TEST(Exception, LightweightConstructorSkipsStackTrace) {
  const muduo::Exception ex("lite", std::source_location::current());
  EXPECT_STREQ(ex.what(), "lite");
  EXPECT_TRUE(ex.stackTraceView().empty());
  EXPECT_NE(ex.fileName(), nullptr);
  EXPECT_NE(ex.functionName(), nullptr);
  EXPECT_GT(ex.line(), 0u);
}

TEST(Exception, ExplicitModeCanSkipOrCapture) {
  const muduo::Exception skip("skip", muduo::Exception::StackTraceMode::Skip);
  EXPECT_TRUE(skip.stackTraceView().empty());

  const muduo::Exception capture("capture",
                                 muduo::Exception::StackTraceMode::Capture);
  EXPECT_FALSE(capture.stackTraceView().empty());
}

TEST(Exception, ThrowCatch) {
  try {
    throw muduo::Exception("boom");
  } catch (const muduo::Exception &ex) {
    EXPECT_STREQ(ex.what(), "boom");
    EXPECT_FALSE(ex.stackTraceView().empty());
  }
}
