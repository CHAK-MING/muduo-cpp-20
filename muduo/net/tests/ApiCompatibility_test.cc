#include "muduo/net/Buffer.h"
#include "muduo/net/TcpConnection.h"

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <span>
#include <string>

namespace muduo::net {
namespace {

template <typename T>
concept SupportsLegacySendOverloads = requires(T &conn, const T &constConn,
                                               const std::string &s,
                                               std::string &&r, const char *cstr,
                                               std::string_view sv,
                                               std::span<const std::byte> bytes) {
  conn.send(cstr);
  conn.send(s);
  conn.send(std::move(r));
  conn.send(sv);
  conn.send(bytes);
  constConn.connected();
};

static_assert(SupportsLegacySendOverloads<TcpConnection>);

TEST(ApiCompatibilityTest, BufferAppendLegacyOverloads) {
  Buffer buf;
  const std::string fromString = "alpha";
  const char *fromCStr = "beta";

  buf.append(fromString);
  buf.append(fromCStr);
  buf.append("gamma");

  EXPECT_EQ(buf.retrieveAllAsString(), "alphabetagamma");
}

TEST(ApiCompatibilityTest, BufferAppendByteSpan) {
  Buffer buf;
  constexpr std::array<std::byte, 3> raw{
      std::byte{'o'}, std::byte{'k'}, std::byte{'!'}};

  buf.append(std::span<const std::byte>{raw});
  EXPECT_EQ(buf.retrieveAllAsString(), "ok!");
}

} // namespace
} // namespace muduo::net
