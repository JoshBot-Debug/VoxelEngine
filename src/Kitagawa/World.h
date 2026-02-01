#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "Camera/PerspectiveCamera.h"

#include "Voxel/HeightMap.h"
#include "Voxel/Palette.h"
#include "Voxel/SparseOctree.h"

namespace Kitagawa {

class World {
private:
  uint32_t m_ChunkSize = 0;

  Palette   m_Palette;
  HeightMap m_HeightMap;

  std::vector<std::shared_ptr<Voxel>> m_Voxels;
  SparseOctree<Voxel>*                m_SVO = nullptr;

private:
  const void GenerateCornellBox();

  const void GenerateHeightMapChunk(const glm::ivec3& origin, float step);

public:
  World(uint32_t chunkSize);
  ~World();

  void RenderUI();

  SparseOctree<Voxel>* GetSVO() { return m_SVO; };

  Palette& GetPalette() { return m_Palette; };

  uint32_t GetChunkSize() { return m_ChunkSize; }

  void Flush();
};

} // namespace Kitagawa