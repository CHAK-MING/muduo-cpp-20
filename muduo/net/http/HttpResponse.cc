#include "muduo/net/http/HttpResponse.h"

#include "muduo/net/Buffer.h"

#include <format>

namespace muduo::net {

void HttpResponse::appendToBuffer(Buffer *output) const {
  const auto statusLine =
      std::format("HTTP/1.1 {} ", static_cast<int>(statusCode_));
  output->append(std::string_view(statusLine));
  output->append(std::string_view(statusMessage_));
  output->append(std::string_view("\r\n"));

  if (closeConnection_) {
    output->append(std::string_view("Connection: close\r\n"));
  } else {
    const auto contentLength =
        std::format("Content-Length: {}\r\n", body_.size());
    output->append(std::string_view(contentLength));
    output->append(std::string_view("Connection: Keep-Alive\r\n"));
  }

  for (const auto &[key, value] : headers_) {
    output->append(std::string_view(key));
    output->append(std::string_view(": "));
    output->append(std::string_view(value));
    output->append(std::string_view("\r\n"));
  }

  output->append(std::string_view("\r\n"));
  output->append(std::string_view(body_));
}

} // namespace muduo::net
