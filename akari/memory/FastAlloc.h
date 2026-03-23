#pragma once

#include <algorithm>
#include <deque>
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <vector>

namespace akari::memory {

template <typename T>
class FastAlloc {

private:
  inline static std::deque<T>         m_Nodes;
  inline static std::vector<uint32_t> m_Free;

public:
  template <typename... Args>
  static void* operator new(size_t size, Args&&... args) {

    for (size_t i = 0; i < m_Free.size(); i++) {
      auto& bitmask = m_Free[i];

      if (bitmask == 0)
        continue;

      uint32_t bit   = __builtin_ctz(bitmask);
      uint32_t index = (i << 5) + bit;
      bitmask &= ~(1U << bit);
      return new (&m_Nodes[index]) T(std::forward<Args>(args)...);
    }

    size_t index = m_Nodes.size();

    uint32_t word = index >> 5;
    uint32_t bit  = index & 31;

    if (m_Free.size() == word) {
      m_Free.emplace_back(0xFFFFFFFF);
      m_Nodes.resize(m_Nodes.size() + 32);
    }

    m_Free[word] &= ~(1U << bit);
    std::cout << "Alloc" << std::endl;
    return new (&m_Nodes[index]) T(std::forward<Args>(args)...);
  }

  static void operator delete(void* ptr) {
    auto it = std::find_if(m_Nodes.begin(), m_Nodes.end(), [&ptr](auto& node) {
      return ptr == &node;
    });

    int index = std::distance(m_Nodes.begin(), it);

    uint32_t word = index >> 5;
    uint32_t bit  = index & 31;
    m_Free[word] |= (1U << bit);
    ptr->~T();
    std::cout << "Dealloc" << std::endl;
  }
};
} // namespace akari::memory
