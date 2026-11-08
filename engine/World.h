#pragma once

#include <memory>
#include <vector>

#include "camera/PerspectiveCamera.h"

#include "ChunkManager.h"
#include "voxel/HeightMap.h"
#include "voxel/Palette.h"
#include "voxel/SparseOctree.h"

namespace vxen {
class UI;
}

using WorldChunkManager = vxen::ChunkManager;

struct alignas(16) Ray {
  glm::vec4 Origin;
  glm::vec4 Direction;
  glm::vec4 Color;
};

struct CornellBoxConfig {
  uint32_t    width  = 64;
  uint32_t    height = 64;
  uint32_t    depth  = 64;
  glm::u8vec3 origin = glm::u8vec3(0, 0, 0);

  struct StructuralIncludes {
    bool leftWall  = true;
    bool rightWall = true;
    bool backWall  = true;
    bool frontWall = false;
    bool floor     = true;
    bool ceiling   = true;
    bool cube      = true;
    bool sphere    = true;
  } include;

  struct LightParams {
    bool         include  = true;
    glm::u32vec3 position = glm::u32vec3(32, 60, 32);
  } light;
};

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
  vxen::UI*             m_UI {nullptr};

  std::vector<Material>                 m_Materials {};
  std::vector<uint32_t>                 m_MaterialsLUT {};
  std::vector<SparseOctree::FilterNode> m_Lights {};
  std::vector<Ray>                      m_Rays {};

  const SparseOctree::Hit* m_MouseHit {nullptr};

private:
  const void GenerateCornellBox(const CornellBoxConfig& config);

  const void GenerateChunk(const uint32_t size, const glm::ivec3& coord);

  const void GenerateSphere(const uint32_t size, const glm::ivec3& coord);

  const void GenerateCube(const uint32_t size, const glm::ivec3& coord);

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

  void SetMouseHit(const SparseOctree::Hit* hit) { m_MouseHit = hit; };

  void SetUI(vxen::UI* ui) { m_UI = ui; };

  const std::vector<Material>&                 GetMaterials() { return m_Materials; };
  const std::vector<uint32_t>&                 GetMaterialsLUT() { return m_MaterialsLUT; };
  const std::vector<SparseOctree::FilterNode>& GetLights() { return m_Lights; };
  const std::vector<Ray>&                      GetRays() { return m_Rays; };

  ChunkBuffers GetChunkBuffers();

  SparseOctree::Hit Raycast(const glm::vec2& origin);

  void FlushPreprocessor(VkCommandBuffer commandBuffer);

  void Clean();
};

} // namespace vxen
