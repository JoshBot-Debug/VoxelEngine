
#pragma once

#include <string>

#include "ChunkManager.h"
#include "voxel/Palette.h"

namespace vxen {

class VoxLoader {

private:
  Palette*      m_Palette {nullptr};
  ChunkManager* m_ChunkManager {nullptr};
  float         m_ColorMergeThreshold = 0.0f;

public:
  void SetPalette(Palette* palette);
  void SetChunkManager(ChunkManager* chunkManager);
  void SetColorMergeThreshold(float threshold) { m_ColorMergeThreshold = std::max(0.0f, threshold); }
  
  bool Load(const std::string& path);
};

} // namespace vxen