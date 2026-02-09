#pragma once

#include <atomic>
#include <mutex>
#include <queue>

namespace Akari {
  
class alignas(64) Signal {
private:
  std::atomic<uint64_t> m_Flags = 0;

private:
  static Signal& Instance();

public:
  Signal() = default;

  Signal(const Signal&) = delete;

  Signal& operator=(const Signal&) = delete;

  static void Set(uint64_t flags);

  static bool Consume(uint64_t flags);
};

} // namespace Akari
