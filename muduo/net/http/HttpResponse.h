#pragma once

#include "muduo/base/Types.h"

#include <map>
#include <string_view>

namespace muduo::net {

class Buffer;

class HttpResponse {
public:
  enum class HttpStatusCode {
    kUnknown,
    k200Ok = 200,
    k301MovedPermanently = 301,
    k400BadRequest = 400,
    k404NotFound = 404,
  };

  static constexpr HttpStatusCode kUnknown = HttpStatusCode::kUnknown;
  static constexpr HttpStatusCode k200Ok = HttpStatusCode::k200Ok;
  static constexpr HttpStatusCode k301MovedPermanently =
      HttpStatusCode::k301MovedPermanently;
  static constexpr HttpStatusCode k400BadRequest =
      HttpStatusCode::k400BadRequest;
  static constexpr HttpStatusCode k404NotFound = HttpStatusCode::k404NotFound;

  explicit HttpResponse(bool close) : closeConnection_(close) {}

  void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
  void setStatusMessage(std::string_view message) { statusMessage_.assign(message); }

  void setCloseConnection(bool on) { closeConnection_ = on; }
  [[nodiscard]] bool closeConnection() const { return closeConnection_; }

  void setContentType(std::string_view contentType) {
    addHeader("Content-Type", contentType);
  }

  void addHeader(std::string_view key, std::string_view value) {
    headers_[string(key)] = string(value);
  }

  void setBody(std::string_view body) { body_.assign(body); }

  void appendToBuffer(Buffer *output) const;

private:
  std::map<string, string> headers_;
  HttpStatusCode statusCode_{kUnknown};
  string statusMessage_;
  bool closeConnection_{false};
  string body_;
};

} // namespace muduo::net
