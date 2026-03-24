#pragma once

#include <cstdlib>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>
#include <utility>
#include <vector>

namespace akari::memory {

template <typename T>
class FastAlloc {

  // static_assert(S != 0 && (S & (S - 1)) == 0, "S must be a power of two");

private:
  inline static T*                    m_Nodes {nullptr};
  inline static std::vector<uint32_t> m_Free;
  inline static const uint32_t        BLOCKS {(64 * 64 * 64) * 10};

public:
  FastAlloc() {
    m_Nodes = reinterpret_cast<T*>(malloc(BLOCKS * sizeof(T)));
    m_Free.resize(BLOCKS / 32, 0xFFFFFFFF);
  }

  template <typename... Args>
  static void* operator new(size_t size, Args&&... args) {

    for (size_t i = 0; i < m_Free.size(); i++) {
      uint32_t& free = m_Free[i];
      if (free == 0)
        continue;
      uint32_t bit   = __builtin_ffs(free);
      uint32_t index = (i << 5) + bit;
      free &= ~(1U << bit);
      return static_cast<void*>(new (m_Nodes + index) T(std::forward<Args>(args)...));
    }

    throw std::runtime_error("Expected to never reach here");
  }

  static void operator delete(void* p) {
    free(p);
    T*       ptr   = static_cast<T*>(ptr);
    uint32_t index = static_cast<uint32_t>(ptr - m_Nodes);
    uint32_t word  = index >> 5;
    uint32_t bit   = index & 32;
    m_Free[word] |= 1U << bit;
  }
};
} // namespace akari::memory
