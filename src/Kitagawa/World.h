#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "Camera/PerspectiveCamera.h"

#include "Voxel/HeightMap.h"
#include "Voxel/Palette.h"
#include "Voxel/SparseVoxelOctree.h"

namespace Kitagawa {

class World {
private:
  uint32_t m_ChunkSize = 0;

  Palette m_Palette;

  std::vector<std::shared_ptr<Voxel>> m_Voxels;
  std::shared_ptr<SparseVoxelOctree>  m_SVO = nullptr;

private:
  const void GenerateCornellBox();

public:
  World(uint32_t chunkSize);
  ~World() = default;

  void RenderUI();

  const std::shared_ptr<SparseVoxelOctree>& GetSVO() { return m_SVO; };

  Palette& GetPalette() { return m_Palette; };

  uint32_t GetChunkSize() { return m_ChunkSize; }

  void Flush();
};

} // namespace Kitagawa