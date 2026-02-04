#pragma once

#include <atomic>
#include <new>
#include <vector>
#include <thread>

template <typename T>
class RCU {
private:
  static const uint32_t MAX_GENERATIONS = 1024 * 8;

  struct alignas(std::hardware_destructive_interference_size) Generation {
    std::atomic<uint64_t> Current = 0;
  };

  struct alignas(std::hardware_destructive_interference_size) Reference {
    std::atomic<uint64_t> Ref = 0;
  };

  Generation      m_Generation;
  Reference       m_References[MAX_GENERATIONS];
  std::vector<T*> m_Retired[MAX_GENERATIONS];

public:
  ~RCU() = default;

  /**
   * When a reader calls read lock, we hand them the current generation.
   * This helps us track how many references there are to the current generation.
   *
   * @returns The current generation
   */
  uint64_t ReadLock();

  /**
   * When a reader calls read unlock, they give us back the generation.
   * This helps us track how many references are released.
   * When all references to a generation hit zero, collect garbage (or rather, you can now free all memory from generation x).
   *
   * @param generation The generation retrieved from ReadLock()
   */
  void ReadUnlock(uint64_t generation);

  void Retire(T* data);

  void Sync();
};

template <typename T>
inline uint64_t RCU<T>::ReadLock() {
  uint64_t g;
  for (;;) {
    g = m_Generation.Current.load(std::memory_order::acquire);
    m_References[g].Ref.fetch_add(1, std::memory_order::relaxed);

    // Verify generation did not change
    if (m_Generation.Current.load(std::memory_order::acquire) == g)
      break;

    // Generation changed, undo and retry
    m_References[g].Ref.fetch_sub(1, std::memory_order::relaxed);
  }
  return g;
}

template <typename T>
inline void RCU<T>::ReadUnlock(uint64_t g) {
  m_References[g].Ref.fetch_sub(1, std::memory_order::relaxed);
}

template <typename T>
inline void RCU<T>::Sync() {
  uint64_t previous = m_Generation.Current.load(std::memory_order::relaxed);
  uint64_t next     = previous ^ 1;

  m_Generation.Current.store(next, std::memory_order::release);

  while (m_References[previous].Ref.load(std::memory_order::acquire) != 0)
    std::this_thread::yield();

  for (T* n : m_Retired[previous])
    delete n;

  m_Retired[previous].clear();
}

template <typename T>
inline void RCU<T>::Retire(T* data) {
  uint64_t g = m_Generation.Current.load(std::memory_order::relaxed);
  m_Retired[g].push_back(data);
}