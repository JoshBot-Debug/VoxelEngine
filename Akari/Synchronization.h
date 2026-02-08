#pragma once

#include <atomic>
#include <mutex>
#include <queue>

namespace Akari {
  
class Synchronization {
private:
  std::atomic<uint64_t> m_Flags = 0;

private:
  static Synchronization& Instance();

public:
  Synchronization() = default;

  Synchronization(const Synchronization&) = delete;

  Synchronization& operator=(const Synchronization&) = delete;

  static void Set(uint64_t flags);

  static bool Consume(uint64_t flags);
};

} // namespace Akari
