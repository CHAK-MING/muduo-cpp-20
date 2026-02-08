#include "muduo/base/Types.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#ifndef MUDUO_DISABLE_LEGACY_LOG_MACROS

TEST(StringPiece, BasicOperations) {
  muduo::StringPiece piece("hello world");
  EXPECT_EQ(piece.size(), 11U);
  EXPECT_FALSE(piece.empty());
  EXPECT_EQ(piece.substr(0, 5).as_string(), "hello");

  piece.remove_prefix(6);
  EXPECT_EQ(piece.as_string(), "world");
  piece.remove_suffix(3);
  EXPECT_EQ(piece.as_string(), "wo");
}

TEST(StringArg, AcceptsLegacyInputs) {
  std::string s = "abc";
  muduo::StringPiece sp(s);

  muduo::StringArg a1("x");
  muduo::StringArg a2(s);
  muduo::StringArg a3(sp);
  muduo::StringArg a4(std::string_view("yz"));

  EXPECT_EQ(a1.as_string_view(), "x");
  EXPECT_EQ(a2.as_string_view(), "abc");
  EXPECT_EQ(a3.as_string_view(), "abc");
  EXPECT_EQ(a4.as_string_view(), "yz");
}

#else

TEST(StringPiece, DisabledByMacro) { SUCCEED(); }

#endif

