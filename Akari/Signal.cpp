#include "Signal.h"

namespace Akari {

Signal& Signal::Instance() {
  static Signal instance;
  return instance;
}

void Signal::Set(uint64_t flags) {
  Signal::Instance().m_Flags.fetch_or(flags, std::memory_order::acq_rel);
}

bool Signal::Consume(uint64_t flags) {
  uint64_t current = Signal::Instance().m_Flags.fetch_and(~flags, std::memory_order::acq_rel);
  return (current & flags) != 0;
}

} // namespace Akari