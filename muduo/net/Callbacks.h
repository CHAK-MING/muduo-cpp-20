#pragma once

#include "muduo/base/Types.h"
#include "muduo/base/Timestamp.h"

#include <cassert>
#include <cstddef>
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>

namespace muduo {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

template <typename T>
  requires (!std::is_void_v<T>)
[[nodiscard]] inline T *get_pointer(const std::shared_ptr<T> &ptr) {
  return ptr.get();
}

template <typename T>
  requires (!std::is_void_v<T>)
[[nodiscard]] inline T *get_pointer(const std::unique_ptr<T> &ptr) {
  return ptr.get();
}

template <typename To, typename From>
inline std::shared_ptr<To> down_pointer_cast(const std::shared_ptr<From> &f) {
  static_assert(std::convertible_to<To *, From *>,
                "down_pointer_cast requires To* convertible to From*");

#ifndef NDEBUG
  assert(f == nullptr || dynamic_cast<To *>(get_pointer(f)) != nullptr);
#endif
  return std::static_pointer_cast<To>(f);
}

namespace net {

class Buffer;
class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

#ifndef MUDUO_DISABLE_LEGACY_LOG_MACROS
template <typename Signature>
using CallbackFunction = muduo::detail::MoveOnlyFunction<Signature>;
#else
template <typename Signature>
using CallbackFunction = std::function<Signature>;
#endif

template <typename F, typename Callback>
concept CallbackBindable =
    std::constructible_from<Callback, F> &&
    (!std::same_as<std::remove_cvref_t<F>, Callback>);

using TimerCallback = CallbackFunction<void()>;
using ConnectionCallback = CallbackFunction<void(const TcpConnectionPtr &)>;
using CloseCallback = CallbackFunction<void(const TcpConnectionPtr &)>;
using WriteCompleteCallback = CallbackFunction<void(const TcpConnectionPtr &)>;
using HighWaterMarkCallback =
    CallbackFunction<void(const TcpConnectionPtr &, size_t)>;
using MessageCallback =
    CallbackFunction<void(const TcpConnectionPtr &, Buffer *, Timestamp)>;

void defaultConnectionCallback(const TcpConnectionPtr &conn);
void defaultMessageCallback(const TcpConnectionPtr &conn, Buffer *buffer,
                            Timestamp receiveTime);

} // namespace net
} // namespace muduo
