#pragma once

#include "muduo/net/http/HttpRequest.h"

namespace muduo::net {

class Buffer;

class HttpContext {
public:
  enum class HttpRequestParseState {
    kExpectRequestLine,
    kExpectHeaders,
    kExpectBody,
    kGotAll,
  };

  static constexpr HttpRequestParseState kExpectRequestLine =
      HttpRequestParseState::kExpectRequestLine;
  static constexpr HttpRequestParseState kExpectHeaders =
      HttpRequestParseState::kExpectHeaders;
  static constexpr HttpRequestParseState kExpectBody =
      HttpRequestParseState::kExpectBody;
  static constexpr HttpRequestParseState kGotAll =
      HttpRequestParseState::kGotAll;

  HttpContext() = default;

  [[nodiscard]] bool parseRequest(Buffer *buf, Timestamp receiveTime);

  [[nodiscard]] bool gotAll() const {
    return state_ == kGotAll;
  }

  void reset() {
    state_ = kExpectRequestLine;
    HttpRequest dummy;
    request_.swap(dummy);
  }

  [[nodiscard]] const HttpRequest &request() const { return request_; }
  [[nodiscard]] HttpRequest &request() { return request_; }

private:
  [[nodiscard]] bool processRequestLine(std::string_view line);

  HttpRequestParseState state_{kExpectRequestLine};
  HttpRequest request_;
};

} // namespace muduo::net
