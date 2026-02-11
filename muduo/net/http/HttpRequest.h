#pragma once

#include "muduo/base/Timestamp.h"
#include "muduo/base/Types.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>
#include <map>
#include <string_view>

namespace muduo::net {

class HttpRequest {
public:
  using HeaderMap = std::map<string, string, std::less<>>;
  enum class Method { kInvalid, kGet, kPost, kHead, kPut, kDelete };
  enum class Version { kUnknown, kHttp10, kHttp11 };

  static constexpr Method kInvalid = Method::kInvalid;
  static constexpr Method kGet = Method::kGet;
  static constexpr Method kPost = Method::kPost;
  static constexpr Method kHead = Method::kHead;
  static constexpr Method kPut = Method::kPut;
  static constexpr Method kDelete = Method::kDelete;

  static constexpr Version kUnknown = Version::kUnknown;
  static constexpr Version kHttp10 = Version::kHttp10;
  static constexpr Version kHttp11 = Version::kHttp11;

  HttpRequest() = default;

  void setVersion(Version v) { version_ = v; }
  [[nodiscard]] Version getVersion() const { return version_; }

  [[nodiscard]] bool setMethod(const char *start, const char *end) {
    assert(method_ == kInvalid);
    return setMethod(std::string_view(start, static_cast<size_t>(end - start)));
  }

  [[nodiscard]] bool setMethod(std::string_view m) {
    if (m == "GET") {
      method_ = kGet;
    } else if (m == "POST") {
      method_ = kPost;
    } else if (m == "HEAD") {
      method_ = kHead;
    } else if (m == "PUT") {
      method_ = kPut;
    } else if (m == "DELETE") {
      method_ = kDelete;
    } else {
      method_ = kInvalid;
    }
    return method_ != kInvalid;
  }

  [[nodiscard]] Method method() const { return method_; }

  [[nodiscard]] const char *methodString() const {
    switch (method_) {
    case Method::kGet:
      return "GET";
    case Method::kPost:
      return "POST";
    case Method::kHead:
      return "HEAD";
    case Method::kPut:
      return "PUT";
    case Method::kDelete:
      return "DELETE";
    default:
      return "UNKNOWN";
    }
  }

  void setPath(const char *start, const char *end) {
    path_.assign(start, static_cast<size_t>(end - start));
  }
  void setPath(std::string_view path) { path_.assign(path); }

  [[nodiscard]] const string &path() const { return path_; }

  void setQuery(const char *start, const char *end) {
    query_.assign(start, static_cast<size_t>(end - start));
  }
  void setQuery(std::string_view query) { query_.assign(query); }

  [[nodiscard]] const string &query() const { return query_; }

  void setReceiveTime(Timestamp t) { receiveTime_ = t; }
  [[nodiscard]] Timestamp receiveTime() const { return receiveTime_; }

  void addHeader(const char *start, const char *colon, const char *end) {
    std::string_view key(start, static_cast<size_t>(colon - start));
    std::string_view value(colon + 1, static_cast<size_t>(end - (colon + 1)));
    addHeader(key, value);
  }

  void addHeader(std::string_view key, std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
      value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
      value.remove_suffix(1);
    }
    headers_[string(key)] = string(value);
  }

  [[nodiscard]] string getHeader(std::string_view field) const {
    const auto it = headers_.find(field);
    return it != headers_.end() ? it->second : string{};
  }

  [[nodiscard]] const HeaderMap &headers() const {
    return headers_;
  }

  void swap(HttpRequest &that) noexcept {
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    receiveTime_.swap(that.receiveTime_);
    headers_.swap(that.headers_);
  }

private:
  Method method_{kInvalid};
  Version version_{kUnknown};
  string path_;
  string query_;
  Timestamp receiveTime_;
  HeaderMap headers_;
};

} // namespace muduo::net
