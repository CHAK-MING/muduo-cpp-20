#pragma once

#include <cassert>
#include <concepts>
#include <cstdlib>
#include <mutex>

namespace muduo {

namespace detail {
template <typename T>
concept HasNoDestroy = requires { &T::no_destroy; };
} // namespace detail

template <typename T>
  requires std::default_initializable<T>
class Singleton {
public:
  Singleton() = delete;
  ~Singleton() = delete;

  static T &instance() {
    std::call_once(once_, &Singleton::init);
    assert(value_ != nullptr);
    return *value_;
  }

private:
  static void init() {
    value_ = new T();
    if constexpr (!detail::HasNoDestroy<T>) {
      std::atexit(&Singleton::destroy);
    }
  }

  static void destroy() {
    delete value_;
    value_ = nullptr;
  }

  inline static std::once_flag once_{};
  inline static T *value_ = nullptr;
};

} // namespace muduo
