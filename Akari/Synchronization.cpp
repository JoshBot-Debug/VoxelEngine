#include "Synchronization.h"

namespace Akari {

Synchronization& Synchronization::Instance() {
  static Synchronization instance;
  return instance;
}

void Synchronization::Set(uint64_t flags) {
  Synchronization::Instance().m_Flags.fetch_or(flags, std::memory_order::acq_rel);
}

bool Synchronization::Consume(uint64_t flags) {
  uint64_t current = Synchronization::Instance().m_Flags.fetch_and(~flags, std::memory_order::acq_rel);
  return (current & flags) != 0;
}

} // namespace Akari