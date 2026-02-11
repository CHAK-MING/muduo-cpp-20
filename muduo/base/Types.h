#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

#include "muduo/base/CxxFeatures.h"

namespace muduo {

using std::string;

constexpr void memZero(void *p, size_t n) { std::memset(p, 0, n); }
constexpr void memZero(std::span<std::byte> bytes) {
  std::ranges::fill(bytes, std::byte{0});
}

template <typename To, typename From>
concept ImplicitlyCastable = std::convertible_to<const From &, To>;

template <typename To, typename From>
  requires ImplicitlyCastable<To, From>
constexpr To implicit_cast(const From &f) {
  return f;
}

template <typename To, typename From>
concept DownCastTarget = std::is_pointer_v<To> && std::is_pointer_v<From *> &&
                         std::convertible_to<To, From *>;

template <typename To, typename From>
  requires DownCastTarget<To, From>
constexpr To down_cast(From *f) {
#if !defined(NDEBUG)
  if constexpr (std::is_polymorphic_v<From>) {
    assert(f == nullptr || dynamic_cast<To>(f) != nullptr);
  }
#endif
  return static_cast<To>(f);
}

namespace detail {

#if MUDUO_HAS_CPP23_MOVE_ONLY_FUNCTION
template <typename Signature>
using MoveOnlyFunction = std::move_only_function<Signature>;
#else

template <typename Signature> class MoveOnlyFunction;

template <typename R, typename... Args> class MoveOnlyFunction<R(Args...)> {
public:
  MoveOnlyFunction() = default;
  explicit MoveOnlyFunction(std::nullptr_t) {}

  MoveOnlyFunction(const MoveOnlyFunction &) = delete;
  MoveOnlyFunction &operator=(const MoveOnlyFunction &) = delete;
  MoveOnlyFunction(MoveOnlyFunction &&) noexcept = default;
  MoveOnlyFunction &operator=(MoveOnlyFunction &&) noexcept = default;

  template <typename F>
    requires(!std::same_as<std::remove_cvref_t<F>, MoveOnlyFunction> &&
             std::is_invocable_r_v<R, std::decay_t<F> &, Args...>)
  explicit MoveOnlyFunction(F &&f)
      : callable_(
            std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(f))) {}

  explicit operator bool() const noexcept {
    return static_cast<bool>(callable_);
  }

  R operator()(Args... args) const {
    assert(callable_ != nullptr);
    return callable_->call(std::forward<Args>(args)...);
  }

private:
  struct Concept {
    virtual ~Concept() = default;
    virtual R call(Args &&...args) const = 0;
  };

  template <typename F> struct Model final : Concept {
    explicit Model(F f) : function_(std::move(f)) {}

    R call(Args &&...args) const override {
      if constexpr (std::is_void_v<R>) {
        std::invoke(function_, std::forward<Args>(args)...);
      } else {
        return std::invoke(function_, std::forward<Args>(args)...);
      }
    }

    mutable F function_;
  };

  std::unique_ptr<Concept> callable_;
};

#endif

} // namespace detail

} // namespace muduo
