#include "muduo/net/InetAddress.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>

using muduo::net::InetAddress;
using namespace std::string_view_literals;

namespace {

struct AddressTextCase {
  std::string ip;
  std::uint16_t port;
  std::string ipPort;
  bool ipv6;
};

class InetAddressTextTest : public ::testing::TestWithParam<AddressTextCase> {};

TEST_P(InetAddressTextTest, ParseAndFormat) {
  const auto &tc = GetParam();
  const InetAddress addr(std::string_view{tc.ip}, tc.port, tc.ipv6);

  EXPECT_EQ(addr.toIp(), tc.ip);
  EXPECT_EQ(addr.toIpPort(), tc.ipPort);
  EXPECT_EQ(addr.port(), tc.port);
  EXPECT_EQ(addr.isIpv6(), tc.ipv6);
}

INSTANTIATE_TEST_SUITE_P(
    TextCases, InetAddressTextTest,
    ::testing::Values(
        AddressTextCase{"1.2.3.4", 8888, "1.2.3.4:8888", false},
        AddressTextCase{"255.254.253.252", 65535, "255.254.253.252:65535",
                        false},
        AddressTextCase{"2001:db8::1", 8888, "[2001:db8::1]:8888", true},
        AddressTextCase{"fe80::1234:abcd:1", 8888, "[fe80::1234:abcd:1]:8888",
                        true}));

} // namespace

TEST(InetAddressTest, IPv4Basics) {
  const InetAddress addr0(1234);
  EXPECT_EQ(addr0.toIp(), "0.0.0.0");
  EXPECT_EQ(addr0.toIpPort(), "0.0.0.0:1234");
  EXPECT_EQ(addr0.port(), 1234);

  const InetAddress addr1(4321, true);
  EXPECT_EQ(addr1.toIp(), "127.0.0.1");
  EXPECT_EQ(addr1.toIpPort(), "127.0.0.1:4321");
  EXPECT_EQ(addr1.port(), 4321);
}

TEST(InetAddressTest, IPv6Basics) {
  const InetAddress addr0(1234, false, true);
  EXPECT_EQ(addr0.toIp(), "::");
  EXPECT_EQ(addr0.toIpPort(), "[::]:1234");
  EXPECT_EQ(addr0.port(), 1234);

  const InetAddress addr1(1234, true, true);
  EXPECT_EQ(addr1.toIp(), "::1");
  EXPECT_EQ(addr1.toIpPort(), "[::1]:1234");
  EXPECT_EQ(addr1.port(), 1234);
}

TEST(InetAddressTest, ResolveLocalhost) {
  InetAddress addr(80);
  EXPECT_TRUE(InetAddress::resolve("localhost"sv, &addr));
  EXPECT_EQ(addr.port(), 80);
}

TEST(InetAddressTest, ResolveExternalDnsOptional) {
  InetAddress addr(80);
  const bool resolved = InetAddress::resolve("google.com"sv, &addr);
  if (resolved) {
    EXPECT_FALSE(addr.toIp().empty());
    EXPECT_EQ(addr.port(), 80);
  } else {
    SUCCEED();
  }
}

TEST(InetAddressTest, FamilyAndVersionHelpers) {
  const InetAddress ipv4("127.0.0.1"sv, 8080);
  EXPECT_TRUE(ipv4.isIpv4());
  EXPECT_FALSE(ipv4.isIpv6());
  EXPECT_EQ(ipv4.family(), AF_INET);

  const InetAddress ipv6("::1"sv, 8080, true);
  EXPECT_FALSE(ipv6.isIpv4());
  EXPECT_TRUE(ipv6.isIpv6());
  EXPECT_EQ(ipv6.family(), AF_INET6);
}
