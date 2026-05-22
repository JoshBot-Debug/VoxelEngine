#pragma once

#include <cstdint>

namespace akari::allocator {

template <typename T>
concept Allocator = requires(T& t) {
  typename T::value_type;
  t.allocate(1024);
  t.deallocate(t.allocate(1024), 1024);
};

template <typename T>
class Greedy {
public:
  typedef T value_type;

  Greedy() = default;

  constexpr T* allocate(std::size_t n);

  constexpr void deallocate(T* p, std::size_t n);
};

template <typename T>
inline constexpr T* allocator::Greedy<T>::allocate(std::size_t n) {
  return ::operator new(n * sizeof(T));
}

template <typename T>
inline constexpr void allocator::Greedy<T>::deallocate(T* p, std::size_t n) {
  for (size_t i = 0; i < n; i++)
    ::operator delete(p);
}

} // namespace akari::allocator
