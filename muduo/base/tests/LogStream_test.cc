#include "muduo/base/LogStream.h"

#include <gtest/gtest.h>

#include <limits>
#include <string>

TEST(LogStream, BooleanIntegerFloatAndString) {
  muduo::LogStream stream;
  const auto &buf = stream.buffer();

  stream << true << ' ' << 42 << ' ' << -7 << ' ' << 0.25 << ' ' << "ok";
  EXPECT_EQ(buf.toString(), "1 42 -7 0.25 ok");
}

TEST(LogStream, IntegerLimitsAndPointer) {
  muduo::LogStream stream;
  const auto &buf = stream.buffer();

  stream << std::numeric_limits<int64_t>::min() << ' '
         << std::numeric_limits<uint64_t>::max() << ' ' << static_cast<void *>(nullptr);
  const std::string s = buf.toString();
  EXPECT_NE(s.find("-9223372036854775808"), std::string::npos);
  EXPECT_NE(s.find("18446744073709551615"), std::string::npos);
  EXPECT_NE(s.find("0x"), std::string::npos);
}

TEST(LogStream, FormatSIAndIEC) {
  EXPECT_EQ(muduo::formatSI(999), "999");
  EXPECT_EQ(muduo::formatSI(1000), "1.00k");
  EXPECT_EQ(muduo::formatIEC(1023), "1023");
  EXPECT_EQ(muduo::formatIEC(1024), "1.00Ki");
}
