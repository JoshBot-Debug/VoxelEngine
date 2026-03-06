#pragma once

#include "thread/Signal.h"

using TSignal = akari::thread::Signal<8>;

enum SignalZero : uint64_t {
  PALETTE_FLUSH_UPDATE = 1ULL << 0,

  CHUNK_MANAGER_SYNC_UPDATE  = 1ULL << 1,
  CHUNK_MANAGER_FLUSH_UPDATE = 1ULL << 2,
  CHUNK_MANAGER_FLUSH_RENDER = 1ULL << 3,
};