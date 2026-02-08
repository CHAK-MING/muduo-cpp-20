#pragma once

#include <concepts>
#include <muduo/base/noncopyable.h>

namespace muduo {

template <typename T>
  requires std::default_initializable<T>
class Singleton : noncopyable {
public:
  static T &instance() {
    static T value{};
    return value;
  }
};

} // namespace muduo
