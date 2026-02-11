#include "muduo/net/SocketsOps.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

namespace {

class SocketPair {
public:
  SocketPair() {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds_) != 0) {
      fds_[0] = -1;
      fds_[1] = -1;
    }
  }

  ~SocketPair() {
    if (fds_[0] >= 0) {
      ::close(fds_[0]);
    }
    if (fds_[1] >= 0) {
      ::close(fds_[1]);
    }
  }

  [[nodiscard]] bool valid() const { return fds_[0] >= 0 && fds_[1] >= 0; }
  [[nodiscard]] int left() const { return fds_[0]; }
  [[nodiscard]] int right() const { return fds_[1]; }

private:
  int fds_[2]{-1, -1};
};

} // namespace

TEST(SocketsOpsTest, SpanReadWrite) {
  SocketPair pair;
  ASSERT_TRUE(pair.valid());

  constexpr std::string_view kMsg = "muduo-span";
  auto writeBytes = std::as_bytes(std::span{kMsg.data(), kMsg.size()});
  const ssize_t wn = muduo::net::sockets::write(pair.left(), writeBytes);
  ASSERT_EQ(wn, static_cast<ssize_t>(kMsg.size()));

  std::array<std::byte, 64> in{};
  const ssize_t rn = muduo::net::sockets::read(pair.right(), std::span{in});
  ASSERT_EQ(rn, static_cast<ssize_t>(kMsg.size()));

  std::array<char, 64> out{};
  std::memcpy(out.data(), in.data(), static_cast<size_t>(rn));
  EXPECT_EQ(std::string_view(out.data(), static_cast<size_t>(rn)), kMsg);
}
