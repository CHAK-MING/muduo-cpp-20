#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace muduo {

template <typename CLASS, typename FUNC> class WeakCallback {
public:
  WeakCallback(const std::weak_ptr<CLASS> &object, FUNC function)
      : object_(object), function_(std::move(function)) {}

  template <typename... Args>
    requires std::invocable<const FUNC &, CLASS *, Args...>
  void operator()(Args &&...args) const {
    if (std::shared_ptr<CLASS> ptr = object_.lock(); ptr) {
      std::invoke(function_, ptr.get(), std::forward<Args>(args)...);
    }
  }

private:
  std::weak_ptr<CLASS> object_;
  [[no_unique_address]] FUNC function_;
};

template <typename CLASS, typename FUNC>
[[nodiscard]] auto makeWeakCallback(const std::shared_ptr<CLASS> &object,
                                    FUNC &&function) {
  using DecayedFunc = std::decay_t<FUNC>;
  return WeakCallback<CLASS, DecayedFunc>(object, std::forward<FUNC>(function));
}

} // namespace muduo
