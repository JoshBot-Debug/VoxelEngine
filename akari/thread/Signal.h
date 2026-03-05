#pragma once

#include <atomic>
#include <mutex>
#include <queue>

namespace akari::thread {

template <uint64_t Words>
class alignas(64) Signal {

private:
  std::atomic<uint64_t> m_Flags[Words] {0};

private:
  static Signal& Instance();

public:
  Signal() = default;

  Signal(const Signal&) = delete;

  Signal& operator=(const Signal&) = delete;

  static void Set(uint32_t word, uint64_t flags);

  static bool Consume(uint32_t word, uint64_t flags);
};

template <uint64_t T>
inline Signal<T>& Signal<T>::Instance() {
  static Signal instance;
  return instance;
}

template <uint64_t T>
inline void Signal<T>::Set(uint32_t word, uint64_t flags) {
  Signal<T>::Instance().m_Flags[word].fetch_or(flags, std::memory_order::acq_rel);
}

template <uint64_t T>
inline bool Signal<T>::Consume(uint32_t word, uint64_t flags) {
  uint64_t current = Signal::Instance().m_Flags[word].fetch_and(~flags, std::memory_order::acq_rel);
  return (current & flags) != 0;
}

} // namespace akari::thread
