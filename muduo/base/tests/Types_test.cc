#include "muduo/base/Types.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <type_traits>

namespace {

struct Base {
  virtual ~Base() = default;
  int value = 1;
};

struct Derived : Base {
  int other = 2;
};

} // namespace

TEST(Types, MemZeroPointerAndSpan) {
  std::array<char, 8> buf{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
  muduo::memZero(buf.data(), buf.size());
  for (char c : buf) {
    EXPECT_EQ(c, '\0');
  }

  std::array<std::byte, 4> bytes{
      std::byte{0x1}, std::byte{0x2}, std::byte{0x3}, std::byte{0x4}};
  muduo::memZero(std::span<std::byte>(bytes));
  for (std::byte b : bytes) {
    EXPECT_EQ(b, std::byte{0});
  }
}

TEST(Types, ImplicitCastAndDownCast) {
  const double d = 3.5;
  const auto x = muduo::implicit_cast<double>(d);
  EXPECT_DOUBLE_EQ(x, 3.5);

  Derived derived;
  Base* base = &derived;
  Derived* casted = muduo::down_cast<Derived*>(base);
  ASSERT_NE(casted, nullptr);
  EXPECT_EQ(casted->other, 2);
}
