#include "StateMachine.h"

namespace akari::state {

bool StateMachine::TrySet(uint64_t state) {
  int64_t expected = 0;

  if (!m_References.compare_exchange_strong(
          expected,
          -1,
          std::memory_order_acquire,
          std::memory_order_relaxed))
    return false;

  m_State.store(state, std::memory_order_release);

  m_References.store(0, std::memory_order_release);

  return true;
}

void StateMachine::Set(uint64_t state) {
  while (!TrySet(state))
    std::this_thread::yield();
}

StateMachine::ReadLock StateMachine::Lock() {
  return StateMachine::ReadLock {this};
}

} // namespace akari::state
