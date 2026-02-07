#include "muduo/base/Exception.h"

#include <gtest/gtest.h>

TEST(Exception, StoresMessageAndStack) {
  muduo::Exception ex("oops");
  EXPECT_STREQ(ex.what(), "oops");
  EXPECT_EQ(ex.whatView(), "oops");
  EXPECT_FALSE(ex.stackTraceView().empty());
}

TEST(Exception, ThrowCatch) {
  try {
    throw muduo::Exception("boom");
  } catch (const muduo::Exception &ex) {
    EXPECT_STREQ(ex.what(), "boom");
    EXPECT_FALSE(ex.stackTraceView().empty());
  }
}
