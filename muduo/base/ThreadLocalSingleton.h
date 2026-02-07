#pragma once

#include <concepts>
#include <memory>

namespace muduo {

template <typename T>
  requires std::default_initializable<T>
class ThreadLocalSingleton {
public:
  ThreadLocalSingleton() = delete;
  ~ThreadLocalSingleton() = delete;

  static T &instance() {
    if (!t_value_) {
      t_value_ = std::make_unique<T>();
    }
    return *t_value_;
  }

  static T *pointer() { return t_value_.get(); }

private:
  inline static thread_local std::unique_ptr<T> t_value_{};
};

} // namespace muduo
