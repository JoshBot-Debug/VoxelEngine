
#pragma once

#include <string>

#include "ChunkManager.h"
#include "voxel/Palette.h"
#include "voxel/Type.h"

namespace vxen {

class VoxLoader {

private:
  Palette*      m_Palette {nullptr};
  ChunkManager* m_ChunkManager {nullptr};

public:
  void SetPalette(Palette* palette);
  void SetChunkManager(ChunkManager* chunkManager);

  bool Load(const std::string& path);
};

} // namespace vxen