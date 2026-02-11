#include "muduo/base/LogStream.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

TEST(LogStreamCompatibility, BooleanIntegerFloatAndString) {
  muduo::LogStream stream;
  const auto &buf = stream.buffer();

  stream << true << ' ' << 42 << ' ' << -7 << ' ' << 0.25 << ' ' << "ok";
  EXPECT_EQ(buf.toString(), "1 42 -7 0.25 ok");
}

TEST(LogStreamCompatibility, IntegerLimitsAndPointer) {
  muduo::LogStream stream;
  const auto &buf = stream.buffer();

  stream << std::numeric_limits<int64_t>::min() << ' '
         << std::numeric_limits<uint64_t>::max() << ' ' << static_cast<void *>(nullptr);
  const std::string s = buf.toString();
  EXPECT_NE(s.find("-9223372036854775808"), std::string::npos);
  EXPECT_NE(s.find("18446744073709551615"), std::string::npos);
  EXPECT_NE(s.find("0x"), std::string::npos);
}

TEST(LogStreamCompatibility, FormatSIAndIEC) {
  EXPECT_EQ(muduo::formatSI(0), "0");
  EXPECT_EQ(muduo::formatSI(999), "999");
  EXPECT_EQ(muduo::formatSI(1000), "1.00k");
  EXPECT_EQ(muduo::formatSI(9994), "9.99k");
  EXPECT_EQ(muduo::formatSI(9995), "10.0k");
  EXPECT_EQ(muduo::formatSI(10049), "10.0k");
  EXPECT_EQ(muduo::formatSI(10050), "10.1k");
  EXPECT_EQ(muduo::formatSI(999999), "1.00M");
  EXPECT_EQ(muduo::formatSI(1000000), "1.00M");
  EXPECT_EQ(muduo::formatSI(std::numeric_limits<int64_t>::max()), "9.22E");
  EXPECT_EQ(muduo::formatIEC(1023), "1023");
  EXPECT_EQ(muduo::formatIEC(1024), "1.00Ki");
  EXPECT_EQ(muduo::formatIEC(10234), "9.99Ki");
  EXPECT_EQ(muduo::formatIEC(10235), "10.0Ki");
  EXPECT_EQ(muduo::formatIEC(1048063), "1023Ki");
  EXPECT_EQ(muduo::formatIEC(1048064), "1.00Mi");
  EXPECT_EQ(muduo::formatIEC(1024 * 1024 - 1), "1.00Mi");
  EXPECT_EQ(muduo::formatIEC(1024 * 1024), "1.00Mi");
}

TEST(LogStreamCompatibility, IntegerTypeLimitsCoverage) {
  muduo::LogStream stream;
  stream << std::numeric_limits<int16_t>::min() << ' '
         << std::numeric_limits<int16_t>::max() << ' '
         << std::numeric_limits<uint16_t>::max() << ' '
         << std::numeric_limits<int32_t>::min() << ' '
         << std::numeric_limits<int32_t>::max() << ' '
         << std::numeric_limits<uint32_t>::max();
  const std::string s = stream.buffer().toString();
  EXPECT_NE(s.find("-32768"), std::string::npos);
  EXPECT_NE(s.find("32767"), std::string::npos);
  EXPECT_NE(s.find("65535"), std::string::npos);
  EXPECT_NE(s.find("-2147483648"), std::string::npos);
  EXPECT_NE(s.find("2147483647"), std::string::npos);
  EXPECT_NE(s.find("4294967295"), std::string::npos);
}

TEST(LogStreamCompatibility, FloatingPointPrecisionCoverage) {
  muduo::LogStream stream;
  stream << 0.05 << ' ' << 0.15 << ' ' << 1.23456789;
  const std::string s = stream.buffer().toString();
  EXPECT_NE(s.find("0.05"), std::string::npos);
  EXPECT_NE(s.find("0.15"), std::string::npos);
  EXPECT_NE(s.find("1.23456789"), std::string::npos);
}

TEST(LogStreamCompatibility, FixedBufferAppendRespectsCapacityBoundary) {
  muduo::detail::FixedBuffer<8> buf;
  buf.append(std::string_view{"12345678"});
  EXPECT_EQ(buf.length(), 8);
  EXPECT_EQ(buf.avail(), 0);
  EXPECT_EQ(buf.toString(), "12345678");
}

TEST(LogStreamCompatibility, FixedBufferAppendTruncatesWhenFull) {
  muduo::detail::FixedBuffer<8> buf;
  buf.append(std::string_view{"12345"});
  buf.append(std::string_view{"6789"});
  EXPECT_EQ(buf.length(), 8);
  EXPECT_EQ(buf.toString(), "12345678");
}

TEST(LogStreamCompatibility, StringViewAppendAtExactSmallBufferBoundary) {
  muduo::LogStream stream;
  std::string payload(muduo::detail::kSmallBuffer, 'x');
  stream << std::string_view(payload);
  EXPECT_EQ(stream.buffer().length(), static_cast<int>(muduo::detail::kSmallBuffer));
}
