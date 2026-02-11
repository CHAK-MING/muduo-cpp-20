#include "muduo/net/Buffer.h"
#include "muduo/net/http/HttpContext.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace muduo::net {
namespace {

using namespace std::string_literals;

struct RequestCase {
  std::string raw;
  HttpRequest::Method method;
  HttpRequest::Version version;
  std::string path;
  std::string host;
  std::string userAgent;
};

class HttpRequestParseTest : public ::testing::TestWithParam<RequestCase> {};

TEST_P(HttpRequestParseTest, ParseAllInOne) {
  const auto &tc = GetParam();
  HttpContext context;
  Buffer input;
  input.append(tc.raw);

  ASSERT_TRUE(context.parseRequest(&input, Timestamp::now()));
  ASSERT_TRUE(context.gotAll());

  const HttpRequest &request = context.request();
  EXPECT_EQ(request.method(), tc.method);
  EXPECT_EQ(request.path(), tc.path);
  EXPECT_EQ(request.getVersion(), tc.version);
  EXPECT_EQ(request.getHeader("Host"), tc.host);
  EXPECT_EQ(request.getHeader("User-Agent"), tc.userAgent);
}

INSTANTIATE_TEST_SUITE_P(
    Requests, HttpRequestParseTest,
    ::testing::Values(
        RequestCase{"GET /index.html HTTP/1.1\r\nHost: www.chenshuo.com\r\n\r\n",
                    HttpRequest::Method::kGet, HttpRequest::Version::kHttp11,
                    "/index.html"s, "www.chenshuo.com"s, ""s},
        RequestCase{
            "GET /index.html HTTP/1.1\r\nHost: www.chenshuo.com\r\nUser-Agent:\r\nAccept-Encoding: \r\n\r\n",
            HttpRequest::Method::kGet, HttpRequest::Version::kHttp11,
            "/index.html"s, "www.chenshuo.com"s, ""s}));

class HttpRequestSplitParseTest : public ::testing::TestWithParam<std::size_t> {};

TEST_P(HttpRequestSplitParseTest, ParseInTwoPieces) {
  constexpr std::string_view all =
      "GET /index.html HTTP/1.1\r\nHost: www.chenshuo.com\r\n\r\n";
  const auto first = GetParam();

  HttpContext context;
  Buffer input;

  input.append(all.substr(0, first));
  ASSERT_TRUE(context.parseRequest(&input, Timestamp::now()));
  ASSERT_FALSE(context.gotAll());

  input.append(all.substr(first));
  ASSERT_TRUE(context.parseRequest(&input, Timestamp::now()));
  ASSERT_TRUE(context.gotAll());

  const HttpRequest &request = context.request();
  EXPECT_EQ(request.method(), HttpRequest::Method::kGet);
  EXPECT_EQ(request.path(), "/index.html");
  EXPECT_EQ(request.getVersion(), HttpRequest::Version::kHttp11);
  EXPECT_EQ(request.getHeader("Host"), "www.chenshuo.com");
  EXPECT_EQ(request.getHeader("User-Agent"), "");
}

INSTANTIATE_TEST_SUITE_P(
    SplitPoints, HttpRequestSplitParseTest,
    ::testing::Values(0U, 1U, 2U, 3U, 4U, 5U, 8U, 12U, 16U, 24U, 32U, 40U,
                      48U));

} // namespace
} // namespace muduo::net
