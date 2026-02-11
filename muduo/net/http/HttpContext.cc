#include "muduo/net/http/HttpContext.h"

#include "muduo/net/Buffer.h"

#include <algorithm>

namespace muduo::net {

bool HttpContext::processRequestLine(std::string_view line) {
  const auto methodEnd = line.find(' ');
  if (methodEnd == std::string_view::npos ||
      !request_.setMethod(line.substr(0, methodEnd))) {
    return false;
  }

  const auto uriBegin = methodEnd + 1;
  const auto versionBegin = line.find(' ', uriBegin);
  if (versionBegin == std::string_view::npos) {
    return false;
  }

  const auto uri = line.substr(uriBegin, versionBegin - uriBegin);
  const auto questionPos = uri.find('?');
  if (questionPos != std::string_view::npos) {
    request_.setPath(uri.substr(0, questionPos));
    request_.setQuery(uri.substr(questionPos + 1));
  } else {
    request_.setPath(uri);
  }

  const auto version = line.substr(versionBegin + 1);
  if (version == "HTTP/1.1") {
    request_.setVersion(HttpRequest::Version::kHttp11);
    return true;
  }
  if (version == "HTTP/1.0") {
    request_.setVersion(HttpRequest::Version::kHttp10);
    return true;
  }
  return false;
}

bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime) {
  bool ok = true;
  bool hasMore = true;

  while (hasMore) {
    if (state_ == HttpRequestParseState::kExpectRequestLine) {
      const auto *crlf = buf->findCRLFChars();
      if (crlf == nullptr) {
        hasMore = false;
      } else {
        const auto readable = buf->readableChars();
        ok = processRequestLine(
            readable.substr(0, static_cast<size_t>(crlf - readable.data())));
        if (ok) {
          request_.setReceiveTime(receiveTime);
          buf->retrieveUntil(crlf + 2);
          state_ = HttpRequestParseState::kExpectHeaders;
        } else {
          hasMore = false;
        }
      }
    } else if (state_ == HttpRequestParseState::kExpectHeaders) {
      const auto *crlf = buf->findCRLFChars();
      if (crlf == nullptr) {
        hasMore = false;
      } else {
        const auto readable = buf->readableChars();
        const auto line =
            readable.substr(0, static_cast<size_t>(crlf - readable.data()));

        const auto colon = line.find(':');
        if (colon != std::string_view::npos) {
          request_.addHeader(line.substr(0, colon), line.substr(colon + 1));
        } else {
          state_ = HttpRequestParseState::kGotAll;
          hasMore = false;
        }
        buf->retrieveUntil(crlf + 2);
      }
    } else if (state_ == HttpRequestParseState::kExpectBody) {
      // No body parsing yet, keep original behavior.
      hasMore = false;
    } else {
      hasMore = false;
    }
  }

  return ok;
}

} // namespace muduo::net
