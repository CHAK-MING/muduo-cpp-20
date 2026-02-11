#include "muduo/net/Buffer.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

using muduo::net::Buffer;
using namespace std::string_view_literals;

namespace {
void appendString(Buffer &buf, const std::string &s) {
  buf.append(std::string_view{s});
}

enum class AppendMode : std::uint8_t {
  kString,
  kStringView,
  kCStr,
  kCharArray,
  kByteSpan,
};

struct AppendCase {
  AppendMode mode;
  std::string payload;
};

class BufferAppendParamTest : public ::testing::TestWithParam<AppendCase> {};

TEST_P(BufferAppendParamTest, AppendsExpectedPayload) {
  const auto &tc = GetParam();
  Buffer buf;

  switch (tc.mode) {
  case AppendMode::kString:
    buf.append(tc.payload);
    break;
  case AppendMode::kStringView:
    buf.append(std::string_view{tc.payload});
    break;
  case AppendMode::kCStr:
    buf.append(std::string_view{tc.payload});
    break;
  case AppendMode::kCharArray:
    buf.append("literal"sv);
    break;
  case AppendMode::kByteSpan:
    buf.append(std::as_bytes(std::span{tc.payload.data(), tc.payload.size()}));
    break;
  }

  if (tc.mode == AppendMode::kCharArray) {
    EXPECT_EQ(buf.retrieveAllAsString(), "literal");
  } else {
    EXPECT_EQ(buf.retrieveAllAsString(), tc.payload);
  }
}

INSTANTIATE_TEST_SUITE_P(
    AppendModes, BufferAppendParamTest,
    ::testing::Values(
        AppendCase{AppendMode::kString, "alpha"},
        AppendCase{AppendMode::kStringView, "bravo"},
        AppendCase{AppendMode::kCStr, "char-pointer"},
        AppendCase{AppendMode::kCharArray, "ignored"},
        AppendCase{AppendMode::kByteSpan, "bytes"}));
} // namespace

TEST(BufferTest, AppendRetrieve) {
  Buffer buf;
  EXPECT_EQ(buf.readableBytes(), 0);
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize);
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend);

  const std::string str(200, 'x');
  appendString(buf, str);
  EXPECT_EQ(buf.readableBytes(), str.size());
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize - str.size());
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend);

  const std::string str2 = buf.retrieveAsString(50);
  EXPECT_EQ(str2.size(), 50);
  EXPECT_EQ(buf.readableBytes(), str.size() - str2.size());
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize - str.size());
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend + str2.size());
  EXPECT_EQ(str2, std::string(50, 'x'));

  appendString(buf, str);
  EXPECT_EQ(buf.readableBytes(), 2 * str.size() - str2.size());
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize - 2 * str.size());
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend + str2.size());

  const std::string str3 = buf.retrieveAllAsString();
  EXPECT_EQ(str3.size(), 350);
  EXPECT_EQ(buf.readableBytes(), 0);
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize);
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend);
  EXPECT_EQ(str3, std::string(350, 'x'));
}

TEST(BufferTest, Grow) {
  Buffer buf;
  appendString(buf, std::string(400, 'y'));
  EXPECT_EQ(buf.readableBytes(), 400);
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize - 400);

  buf.retrieve(50);
  EXPECT_EQ(buf.readableBytes(), 350);
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize - 400);
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend + 50);

  appendString(buf, std::string(1000, 'z'));
  EXPECT_EQ(buf.readableBytes(), 1350);
  EXPECT_EQ(buf.writableBytes(), 0);
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend + 50);

  buf.retrieveAll();
  EXPECT_EQ(buf.readableBytes(), 0);
  EXPECT_EQ(buf.writableBytes(), 1400);
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend);
}

TEST(BufferTest, InsideGrow) {
  Buffer buf;
  appendString(buf, std::string(800, 'y'));
  buf.retrieve(500);
  appendString(buf, std::string(300, 'z'));

  EXPECT_EQ(buf.readableBytes(), 600);
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize - 600);
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend);
}

TEST(BufferTest, Shrink) {
  Buffer buf;
  appendString(buf, std::string(2000, 'y'));
  buf.retrieve(1500);
  buf.shrink(0);

  EXPECT_EQ(buf.readableBytes(), 500);
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize - 500);
  EXPECT_EQ(buf.retrieveAllAsString(), std::string(500, 'y'));
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend);
}

TEST(BufferTest, Prepend) {
  Buffer buf;
  appendString(buf, std::string(200, 'y'));
  EXPECT_EQ(buf.readableBytes(), 200);
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize - 200);
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend);

  int x = 0;
  buf.prepend(&x, sizeof x);
  EXPECT_EQ(buf.readableBytes(), 204);
  EXPECT_EQ(buf.writableBytes(), Buffer::kInitialSize - 200);
  EXPECT_EQ(buf.prependableBytes(), Buffer::kCheapPrepend - 4);
}

TEST(BufferTest, ReadInt) {
  Buffer intbuf;
  intbuf.append("HTTP"sv);
  EXPECT_EQ(intbuf.readableBytes(), 4);
  EXPECT_EQ(intbuf.peekInt8(), 'H');
  const int top16 = intbuf.peekInt16();
  EXPECT_EQ(top16, 'H' * 256 + 'T');
  EXPECT_EQ(intbuf.peekInt32(), top16 * 65536 + 'T' * 256 + 'P');

  EXPECT_EQ(intbuf.readInt8(), 'H');
  EXPECT_EQ(intbuf.readInt16(), 'T' * 256 + 'T');
  EXPECT_EQ(intbuf.readInt8(), 'P');
  EXPECT_EQ(intbuf.readableBytes(), 0);
  EXPECT_EQ(intbuf.writableBytes(), Buffer::kInitialSize);

  intbuf.appendInt8(-1);
  intbuf.appendInt16(-2);
  intbuf.appendInt32(-3);
  EXPECT_EQ(intbuf.readableBytes(), 7);
  EXPECT_EQ(intbuf.readInt8(), -1);
  EXPECT_EQ(intbuf.readInt16(), -2);
  EXPECT_EQ(intbuf.readInt32(), -3);
}

TEST(BufferTest, FindEol) {
  Buffer buf;
  appendString(buf, std::string(100000, 'x'));
  EXPECT_EQ(buf.findEOL(), nullptr);
  EXPECT_EQ(buf.findEOL(buf.peek() + 90000), nullptr);
}

TEST(BufferTest, Move) {
  Buffer moved;
  moved.append("muduo"sv);
  const void *inner = moved.peek();
  Buffer newbuf(std::move(moved));
  EXPECT_EQ(inner, newbuf.peek());
}

TEST(BufferTest, SpanBasedAppendAndWritableSpan) {
  Buffer buf;

  constexpr std::array<std::byte, 4> bytes{
      std::byte{'t'}, std::byte{'e'}, std::byte{'s'}, std::byte{'t'}};
  buf.append(std::span<const std::byte>{bytes});
  EXPECT_EQ(buf.readableBytes(), bytes.size());
  EXPECT_EQ(buf.retrieveAllAsString(), "test");

  auto writable = buf.writableSpan();
  ASSERT_GE(writable.size(), 2u);
  writable[0] = std::byte{'O'};
  writable[1] = std::byte{'K'};
  buf.hasWritten(2);
  EXPECT_EQ(buf.retrieveAllAsString(), "OK");
}

TEST(BufferTest, FindCrLfFromSpanData) {
  Buffer buf;
  buf.append("line1\r\nline2"sv);

  const std::byte *crlf = buf.findCRLF();
  ASSERT_NE(crlf, nullptr);
  buf.retrieveUntil(crlf + 2);
  EXPECT_EQ(buf.retrieveAllAsString(), "line2");
}

TEST(BufferTest, CharViewHelpersForTextProtocols) {
  Buffer buf;
  buf.append("GET / HTTP/1.1\r\nHost: x\r\n\r\n"sv);

  ASSERT_EQ(buf.readableChars().substr(0, 4), "GET ");

  const char *crlf = buf.findCRLFChars();
  ASSERT_NE(crlf, nullptr);
  const auto firstLine =
      buf.readableChars().substr(0, static_cast<size_t>(crlf - buf.peekAsChar()));
  EXPECT_EQ(firstLine, "GET / HTTP/1.1");

  buf.retrieveUntil(crlf + 2);
  EXPECT_EQ(buf.readableChars().substr(0, 7), "Host: x");
}
