#pragma once

#include "thread/Signal.h"

using TSignal = akari::thread::Signal<8>;

enum SignalZero : uint64_t {
  PALETTE_FLUSH_UPDATE = 1ULL << 0,

  /**
   * When the SVO changes are ready to be flushed
   * 1. Call Sync() on dirty chunks
   * 2. Begin preparing vectors & GPU buffers
   */
  CHUNK_MANAGER_FLUSH_UPDATE = 1ULL << 1,

  /**
   * Once CHUNK_MANAGER_FLUSH_UPDATE is done
   * Verticies are ready to be handed over to the draw command.
   */
  CHUNK_MANAGER_FLUSH_VERTICES = 1ULL << 2,

  /**
   * Once CHUNK_MANAGER_FLUSH_UPDATE is done
   * LUTs & FlatNodes for chunks are read to be handed over to the compute shader for culling, etc.
   */
  CHUNK_MANAGER_FLUSH_PREPROCESSOR = 1ULL << 3,
};
