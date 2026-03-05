#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <thread>
#include <vector>

/// TODO: May want to look into Quiescent State-Based Reclamation
namespace akari::state {

template <typename F>
concept SubscriberCallback = requires(F f, void* data) {
  f(data);
};

class StateMachine {
public:
  class ReadLock {
  private:
    StateMachine* m_Self {nullptr};

  public:
    uint64_t Value {0};

  public:
    explicit ReadLock(StateMachine* self) : m_Self(self) {
      int64_t refs;

      while (true) {
        refs = self->m_References.load(std::memory_order_acquire);

        if (refs < 0)
          continue;

        if (self->m_References.compare_exchange_weak(
                refs,
                refs + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed))
          break;
      }

      Value = self->m_State.load(std::memory_order_acquire);
    }

    ~ReadLock() {
      if (m_Self)
        m_Self->m_References.fetch_sub(1, std::memory_order_release);
    }

    ReadLock(const ReadLock&)            = delete;
    ReadLock& operator=(const ReadLock&) = delete;

    ReadLock(ReadLock&& other) noexcept {
      m_Self       = other.m_Self;
      Value        = other.Value;
      other.m_Self = nullptr;
    }
  };

private:
  std::atomic<int64_t> m_References {0};
  std::atomic<uint64_t> m_State;

public:
  ~StateMachine() = default;

  bool TrySet(uint64_t state);

  void Set(uint64_t state);

  ReadLock Lock();

  // template <typename F>
  //   requires SubscriberCallback<F>
  // void Subscribe(Phase phase, uint64_t state, void* data, F callback);
};

// template <typename F>
//   requires SubscriberCallback<F>
// inline void StateMachine::Subscribe(Phase phase, uint64_t state, void* data, F callback) {

// }

} // namespace akari::state