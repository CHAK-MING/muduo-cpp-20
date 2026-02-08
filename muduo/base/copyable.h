#pragma once

namespace muduo {

class copyable {
protected:
  constexpr copyable() = default;
  ~copyable() = default;
};

} // namespace muduo
