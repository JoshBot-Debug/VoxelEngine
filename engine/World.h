#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "camera/PerspectiveCamera.h"

#include "ChunkManager.h"
#include "voxel/HeightMap.h"
#include "voxel/Palette.h"
#include "voxel/SparseOctree.h"

#include "Signal.h"

using WorldChunkManager = vxen::ChunkManager<64, 4>;

namespace vxen {

class World {
private:
  uint32_t m_ChunkSize {0};

  Palette   m_Palette;
  HeightMap m_HeightMap {
      64,
      64,
      TerrainSpecification {
          .Seed        = 50,
          .OctaveCount = 10,
          .Frequency   = 1.0f,
          .Persistence = 0.4f,
          .Scale       = 0.3f,
          .Bias        = -0.4f,
      },
  };

  akari::camera::PerspectiveCamera* m_Camera {nullptr};

  std::vector<std::shared_ptr<Voxel>> m_Voxels {};
  WorldChunkManager*                  m_ChunkManager {nullptr};

  std::vector<Material>                        m_Materials {};
  std::vector<uint32_t>                        m_MaterialsLUT {};
  std::vector<SparseOctree<Voxel>::FilterNode> m_Lights {};

private:
  const void GenerateCornellBox(const glm::u8vec3& origin);

  const void GenerateChunk(const glm::ivec3& coord);

  const void GenerateSphere(const glm::ivec3& coord);

  const void GenerateNoiseSphere(const glm::ivec3& coord);

public:
  World(uint32_t chunkSize);
  ~World();

  void RenderUI();

  void Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport);

  void SetCamera(akari::camera::PerspectiveCamera* camera) { m_Camera = camera; };

  const std::vector<Material>&                        GetMaterials() { return m_Materials; };
  const std::vector<uint32_t>&                        GetMaterialsLUT() { return m_MaterialsLUT; };
  const std::vector<SparseOctree<Voxel>::FilterNode>& GetLights() { return m_Lights; };

  ChunkBuffers GetChunkBuffers();

  WorldChunkManager* GetChunkManager() { return m_ChunkManager; };

  void FlushPreprocessor(VkCommandBuffer commandBuffer);

  void Clean();
};

} // namespace vxen