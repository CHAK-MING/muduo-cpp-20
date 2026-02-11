#include "muduo/base/Types.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#ifndef MUDUO_DISABLE_LEGACY_LOG_MACROS

TEST(StringPiece, BasicOperations) {
  muduo::StringPiece piece("hello world");
  EXPECT_EQ(piece.size(), 11U);
  EXPECT_EQ(piece.sizeInt(), 11);
  EXPECT_FALSE(piece.empty());
  EXPECT_EQ(piece[1], 'e');
  EXPECT_EQ(piece.substr(0, 5).as_string(), "hello");

  piece.remove_prefix(6);
  EXPECT_EQ(piece.as_string(), "world");
  piece.remove_suffix(3);
  EXPECT_EQ(piece.as_string(), "wo");

  std::string copied;
  piece.CopyToString(&copied);
  EXPECT_EQ(copied, "wo");
}

TEST(StringPiece, LegacyCompatMethodsAndComparisons) {
  muduo::StringPiece a("abc");
  muduo::StringPiece b;
  b.set("abcdef", 6);
  EXPECT_TRUE(b.starts_with(a));
  EXPECT_TRUE(b.starts_with('a'));
  EXPECT_TRUE(b.starts_with("abc"));

  const unsigned char raw[] = {'x', 'y', 'z'};
  muduo::StringPiece c;
  c.set(raw, 3);
  EXPECT_EQ(c.as_string(), "xyz");

  muduo::StringPiece d("abd");
  EXPECT_TRUE(a < d);
  EXPECT_TRUE(a <= d);
  EXPECT_TRUE(d > a);
  EXPECT_TRUE(d >= a);
  EXPECT_TRUE(a == muduo::StringPiece("abc"));
  EXPECT_TRUE(a != d);
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
