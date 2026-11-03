#pragma once

#include <memory>
#include <vector>

#include "camera/PerspectiveCamera.h"

#include "ChunkManager.h"
#include "voxel/HeightMap.h"
#include "voxel/Palette.h"
#include "voxel/SparseOctree.h"

using WorldChunkManager = vxen::ChunkManager;

namespace vxen {

class World {
private:
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

  std::vector<uint32_t> m_Voxels {};
  WorldChunkManager*    m_ChunkManager {nullptr};

  std::vector<Material>                 m_Materials {};
  std::vector<uint32_t>                 m_MaterialsLUT {};
  std::vector<SparseOctree::FilterNode> m_Lights {};

private:
  const void GenerateCornellBox(const uint32_t size, const glm::u8vec3& origin);

  const void GenerateChunk(const uint32_t size, const glm::ivec3& coord);

  const void GenerateSphere(const uint32_t size, const glm::ivec3& coord);
  
  const void GenerateCube(const uint32_t size, const glm::ivec3& coord);

  const void GenerateNoiseSphere(const uint32_t size, const glm::ivec3& coord);

public:
  World();
  ~World();

  World(World&&)                 = delete;
  World(const World&)            = delete;
  World& operator=(const World&) = delete;
  World& operator=(World&&)      = delete;

  void RenderUI();

  void Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport);

  void SetCamera(akari::camera::PerspectiveCamera* camera) { m_Camera = camera; };

  const std::vector<Material>&                 GetMaterials() { return m_Materials; };
  const std::vector<uint32_t>&                 GetMaterialsLUT() { return m_MaterialsLUT; };
  const std::vector<SparseOctree::FilterNode>& GetLights() { return m_Lights; };

  ChunkBuffers GetChunkBuffers();

  SparseOctree::Hit Raycast(const glm::vec2& origin);

  void FlushPreprocessor(VkCommandBuffer commandBuffer);

  void Clean();
};

} // namespace vxen
