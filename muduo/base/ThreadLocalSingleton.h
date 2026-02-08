#pragma once

#include "muduo/base/noncopyable.h"

#include <concepts>
#include <optional>

namespace muduo {

template <typename T>
  requires std::default_initializable<T>
class ThreadLocalSingleton : noncopyable {
public:
  static T &instance() {
    if (!t_value_.has_value()) {
      t_value_.emplace();
    }
    return *t_value_;
  }

  static T *pointer() {
    return t_value_.has_value() ? &t_value_.value() : nullptr;
  }

private:
  inline static thread_local std::optional<T> t_value_{};
};

} // namespace muduo
