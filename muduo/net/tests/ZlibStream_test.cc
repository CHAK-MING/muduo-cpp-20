#include "muduo/net/ZlibStream.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <string_view>

using muduo::net::Buffer;
using muduo::net::ZlibInputStream;
using muduo::net::ZlibOutputStream;
using namespace std::string_view_literals;

class ZlibStreamTest : public ::testing::Test {
protected:
  void SetUp() override { std::srand(1); }
};

TEST_F(ZlibStreamTest, EmptyFinishWritesHeaderTrailer) {
  Buffer output;
  {
    ZlibOutputStream stream(&output);
    EXPECT_EQ(output.readableBytes(), 0);
  }
  EXPECT_EQ(output.readableBytes(), 8);
}

TEST_F(ZlibStreamTest, FinishStatus) {
  Buffer output;
  ZlibOutputStream stream(&output);
  EXPECT_EQ(stream.zlibErrorCode(), Z_OK);
  EXPECT_TRUE(stream.finish());
  EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
}

TEST_F(ZlibStreamTest, WriteStringAndFinish) {
  Buffer output;
  ZlibOutputStream stream(&output);
  EXPECT_TRUE(stream.write(
      "01234567890123456789012345678901234567890123456789"sv));
  EXPECT_TRUE(stream.finish());
  EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
  EXPECT_GT(output.readableBytes(), 0);
}

TEST_F(ZlibStreamTest, LargeRepeatedWrites) {
  Buffer output;
  ZlibOutputStream stream(&output);
  for (int i = 0; i < 16384; ++i) {
    ASSERT_TRUE(stream.write(
        "01234567890123456789012345678901234567890123456789"sv));
  }
  EXPECT_TRUE(stream.finish());
  EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
  EXPECT_GT(stream.inputBytes(), stream.outputBytes());
}

TEST_F(ZlibStreamTest, RandomLikeInput) {
  Buffer output;
  ZlibOutputStream stream(&output);
  std::string input;
  input.reserve(32768);
  constexpr const char kAlphabet[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-";
  for (int i = 0; i < 32768; ++i) {
    input.push_back(kAlphabet[std::rand() % 64]);
  }
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(stream.write(input));
  }
  EXPECT_TRUE(stream.finish());
  EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
}

TEST_F(ZlibStreamTest, HugeUnderscoreInput) {
  Buffer output;
  ZlibOutputStream stream(&output);
  std::string input(256 * 1024, '_');
  for (int i = 0; i < 64; ++i) {
    ASSERT_TRUE(stream.write(input));
  }
  EXPECT_TRUE(stream.finish());
  EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
  EXPECT_GT(stream.inputBytes(), 0);
  EXPECT_GT(stream.outputBytes(), 0);
}

TEST_F(ZlibStreamTest, WriteBufferInputInterface) {
  Buffer input;
  input.append("abcdefghabcdefghabcdefgh"sv);
  Buffer compressed;
  ZlibOutputStream stream(&compressed);

  EXPECT_TRUE(stream.write(&input));
  EXPECT_EQ(input.readableBytes(), 0);
  EXPECT_TRUE(stream.finish());
  EXPECT_EQ(stream.zlibErrorCode(), Z_STREAM_END);
  EXPECT_GT(compressed.readableBytes(), 0);
}

TEST_F(ZlibStreamTest, CompressDecompressRoundTripWithBufferInterface) {
  constexpr std::string_view kPayload =
      "muduo-cpp20-zlib-roundtrip-payload-0123456789";

  Buffer rawInput;
  rawInput.append(kPayload);
  Buffer compressed;
  ZlibOutputStream compressor(&compressed);
  ASSERT_TRUE(compressor.write(&rawInput));
  ASSERT_TRUE(compressor.finish());
  ASSERT_EQ(compressor.zlibErrorCode(), Z_STREAM_END);

  Buffer compressedCopy;
  compressedCopy.append(compressed.readableSpan());
  Buffer decompressed;
  ZlibInputStream decompressor(&decompressed);
  ASSERT_TRUE(decompressor.write(&compressedCopy));
  ASSERT_EQ(compressedCopy.readableBytes(), 0);
  ASSERT_TRUE(decompressor.finish());
  ASSERT_TRUE(decompressor.zlibErrorCode() == Z_OK ||
              decompressor.zlibErrorCode() == Z_STREAM_END);
  EXPECT_EQ(decompressed.retrieveAllAsString(), kPayload);
}
