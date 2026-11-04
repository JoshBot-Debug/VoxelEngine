#include "World.h"
#include <imgui.h>

#include "VoxLoader.h"

using namespace akari::thread;

namespace vxen {

World::World() {
  m_ChunkManager = new WorldChunkManager();

  m_HeightMap.Initialize(64, 64);

  m_Palette.Create(Palette::Item {
      .Name = "Left Wall",
      .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}}),
  });

  m_Palette.Create(Palette::Item {
      .Name = "Right Wall",
      .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4 {0.14f, 0.45f, 0.090f, 1.0f}}),
  });

  m_Palette.Create(Palette::Item {
      .Name = "Wall",
      .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4 {0.73f, 0.73f, 0.73f, 1.0f}}),
  });

  m_Palette.Create(Palette::Item {
      .Name = "Cube",
      .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4(1.0f)}),
  });

  m_Palette.Create(Palette::Item {
      .Name = "Sphere",
      .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4(1.0f)}),
  });

  m_Palette.Create(Palette::Item {
      .Name = "Grass Lush",
      .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4(0.30f, 0.60f, 0.25f, 1.0f)}),
  });

  m_Palette.Create(Palette::Item {
      .Name = "Grass Dry",
      .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4(0.55f, 0.60f, 0.30f, 1.0f)}),
  });

  m_Palette.Create(Palette::Item {
      .Name = "Grass Forest",
      .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4(0.22f, 0.42f, 0.18f, 1.0f)}),
  });

  m_Palette.Create(Palette::Item {
      .Name = "Light",
      .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4(1.0f), .Emissive = glm::vec4(1.0f, 1.0f, 1.0f, 3.0f)}),
  });

  TSignal::Set(0, PALETTE_FLUSH_UPDATE);

  CornellBoxConfig config;
  config.width  = 128;
  config.height = 128;
  config.depth  = 128;
  config.light.position.x = 16;
  config.light.position.z = 16;
  config.light.position.y = 70;
  // config.include.ceiling = false;
  GenerateCornellBox(config);

  // auto light = m_Palette.Create(Palette::Item {
  //     .Name = "Light",
  //     .Mat  = std::make_shared<Material>(Material {.Albedo = glm::vec4(1.0f), .Emissive = glm::vec4(1.0f, 1.0f, 1.0f, 3.0f)}),
  // });

  // VoxLoader loader;
  // loader.SetPalette(&m_Palette);
  // loader.SetChunkManager(m_ChunkManager);
  // loader.Load("assets/sponza.vox");

  // m_ChunkManager->Set({110, 110, 110}, light);

  // TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE | PALETTE_FLUSH_UPDATE);
}

World::~World() {
  delete m_ChunkManager;
}

void World::RenderUI() {
  m_Palette.RenderUI();
}

void World::Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport) {

  glm::vec3 rayOrigin    = m_Camera->Position;
  glm::vec3 rayDirection = m_Camera->GetRayDirection(mouse.x, mouse.y);

  // glm::ivec3 wcc = getWorldChunkCoordinate(rayOrigin);
  glm::ivec3 wcc = {0, 0, 0};

  bool isCtrlPressed = ImGui::IsKeyPressed(ImGuiKey_LeftCtrl);
  bool isActing      = ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Left);

  if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
    TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE | PALETTE_FLUSH_UPDATE);
  }

  if (ImGui::IsKeyPressed(ImGuiKey_T)) {
    SparseOctree::Hit hit      = m_ChunkManager->DeepRaymarch(rayOrigin, rayDirection);
    auto              position = m_ChunkManager->WorldToLocalCoordinate(hit.Position + hit.Normal);
    m_ChunkManager->Set(position, hit.Id);
    TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
    SparseOctree::Hit hit      = m_ChunkManager->DeepRaymarch(rayOrigin, rayDirection);
    auto              position = m_ChunkManager->WorldToLocalCoordinate(hit.Position);
    m_ChunkManager->Clear(wcc, position);
    TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
  }

  if (isCtrlPressed && isActing) {
    SparseOctree::Hit hit = m_ChunkManager->DeepRaymarch(rayOrigin, rayDirection);

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
      auto position = m_ChunkManager->WorldToLocalCoordinate(hit.Position);
      m_ChunkManager->Clear(wcc, position);
      TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      auto position = m_ChunkManager->WorldToLocalCoordinate(hit.Position + hit.Normal);
      m_ChunkManager->Set(wcc, position, hit.Id);
      TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
    }
  }

  if (TSignal::Consume(0, PALETTE_FLUSH_UPDATE)) {
    m_Materials            = m_Palette.GetMaterials();
    uint32_t maxMaterialId = 0;

    for (auto& mat : m_Materials)
      maxMaterialId = mat.Id > maxMaterialId ? mat.Id : maxMaterialId;

    m_MaterialsLUT.resize(maxMaterialId + 1);

    for (size_t i = 0; i < m_Materials.size(); i++)
      m_MaterialsLUT[m_Materials[i].Id] = i + 1;
  }

  if (TSignal::Consume(0, CHUNK_MANAGER_FLUSH_UPDATE)) {
    m_ChunkManager->Sync();

    std::vector<uint32_t> ids;

    for (auto& material : m_Palette.GetMaterials())
      ids.push_back(material.Id);

    m_ChunkManager->FlushVertices(ids);

    {
      m_ChunkManager->Filter(m_Lights, [this](const Node* node) {
        auto material = m_Palette.GetMaterial(node->Id);
        return material && material->Emissive.a > 0.0f;
      });
    }
  }
}

ChunkBuffers World::GetChunkBuffers() {
  return m_ChunkManager->GetBuffers();
}

SparseOctree::Hit World::Raycast(const glm::vec2& origin) {
  glm::vec3 rayOrigin    = m_Camera->Position;
  glm::vec3 rayDirection = m_Camera->GetRayDirection(origin.x, origin.y);
  return m_ChunkManager->DeepRaymarch(rayOrigin, rayDirection);
}

void World::FlushPreprocessor(VkCommandBuffer commandBuffer) {
  m_ChunkManager->FlushPreprocessor(commandBuffer);
}

void World::Clean() {
  m_Materials.clear();
  m_MaterialsLUT.clear();
  m_Lights.clear();
}

const void World::GenerateChunk(const uint32_t size, const glm::ivec3& wcc) {
  auto gLush         = m_Palette.Find("Grass Lush");
  auto lightMaterial = m_Palette.Find("Light");

  auto lush  = m_Voxels.emplace_back(gLush->Id);
  auto light = m_Voxels.emplace_back(lightMaterial->Id);

  {
    auto session = m_ChunkManager->BeginWrite(wcc);

    auto noise = m_HeightMap.Build(wcc.x, wcc.x + 1.0f, wcc.z, wcc.z + 1.0f);

    session.Root->Destroy();

    for (int z = 0; z < size; z++)
      for (int x = 0; x < size; x++) {
        float n      = noise.GetValue(x, z);
        int   height = static_cast<int>(std::round((std::clamp(n, -1.0f, 1.0f) + 1) * (size >> 1)));
        for (int y = 0; y < height; y++)
          m_ChunkManager->Set(wcc, session, x, y, z, lush);
      }

    m_ChunkManager->Set(wcc, session, size / 2, size - 4, size / 2, light);
  }
}

const void World::GenerateCornellBox(const CornellBoxConfig& config) {
  auto leftWallMaterial  = m_Palette.Find("Left Wall");
  auto rightWallMaterial = m_Palette.Find("Right Wall");
  auto wallMaterial      = m_Palette.Find("Wall");
  auto cubeMaterial      = m_Palette.Find("Cube");
  auto sphereMaterial    = m_Palette.Find("Sphere");
  auto lightMaterial     = m_Palette.Find("Light");

  auto leftWall  = m_Voxels.emplace_back(leftWallMaterial->Id);
  auto rightWall = m_Voxels.emplace_back(rightWallMaterial->Id);
  auto wall      = m_Voxels.emplace_back(wallMaterial->Id);
  auto cube      = m_Voxels.emplace_back(cubeMaterial->Id);
  auto sphere    = m_Voxels.emplace_back(sphereMaterial->Id);
  auto light     = m_Voxels.emplace_back(lightMaterial->Id);

  // Cache dimensions for cleaner loop syntax
  const uint32_t    w = config.width;
  const uint32_t    h = config.height;
  const uint32_t    d = config.depth;
  const glm::u8vec3 o = config.origin;

  {
    // 1. Lighting Placement (Uses absolute position or origin offset)
    if (config.light.include) {
      m_ChunkManager->Set(o.x + config.light.position.x,
                          o.y + config.light.position.y,
                          o.z + config.light.position.z,
                          light);
    }

    // 2. Shell Wall Generation (Offsets loops using the origin vector)
    if (config.include.floor || config.include.ceiling) {
      for (uint32_t x = 0; x < w; x++) {
        for (uint32_t z = 0; z < d; z++) {
          if (config.include.floor)
            m_ChunkManager->Set(o.x + x, o.y + 0, o.z + z, wall);
          if (config.include.ceiling)
            m_ChunkManager->Set(o.x + x, o.y + h - 1, o.z + z, wall);
        }
      }
    }

    if (config.include.leftWall || config.include.rightWall) {
      for (uint32_t y = 0; y < h; y++) {
        for (uint32_t z = 0; z < d; z++) {
          if (config.include.leftWall)
            m_ChunkManager->Set(o.x + 0, o.y + y, o.z + z, leftWall);
          if (config.include.rightWall)
            m_ChunkManager->Set(o.x + w - 1, o.y + y, o.z + z, rightWall);
        }
      }
    }

    if (config.include.backWall || config.include.frontWall) {
      for (uint32_t x = 0; x < w; x++) {
        for (uint32_t y = 0; y < h; y++) {
          if (config.include.backWall)
            m_ChunkManager->Set(o.x + x, o.y + y, o.z + 0, wall);
          if (config.include.frontWall)
            m_ChunkManager->Set(o.x + x, o.y + y, o.z + d - 1, wall);
        }
      }
    }

    // 3. Proportional Inner Elements
    if (config.include.cube) {
      const glm::ivec2 blockDistanceFromWall = {w / 4, d / 6};
      const int        blockSizeX            = w / 4;
      const int        blockSizeZ            = d / 4;
      const int        blockHeight           = h / 3;

      for (int z = 0; z < blockSizeZ; z++) {
        for (int x = 0; x < blockSizeX; x++) {
          for (int y = 0; y < blockHeight; y++) {
            m_ChunkManager->Set(o.x + x + blockDistanceFromWall.x,
                                o.y + y + 1,
                                o.z + z + blockDistanceFromWall.y,
                                cube);
          }
        }
      }
    }

    if (config.include.sphere) {
      int radius = w / 8;
      int cx     = (w / 4) + (w / 2);
      int cy     = radius;
      int cz     = (d / 6) + (d / 2);

      for (uint32_t z = 0; z < d; ++z) {
        for (uint32_t y = 0; y < h; ++y) {
          for (uint32_t x = 0; x < w; ++x) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float dz = (float)z - cz;
            if (dx * dx + dy * dy + dz * dz <= radius * radius) {
              m_ChunkManager->Set(o.x + x, o.y + y + 1, o.z + z, sphere);
            }
          }
        }
      }
    }
  }

  TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
}

const void World::GenerateSphere(const uint32_t size, const glm::ivec3& wcc) {
  const auto light = m_Voxels.emplace_back(m_Palette.Find("Light")->Id);
  const auto solid = m_Voxels.emplace_back(m_Palette.Find("Grass Lush")->Id);

  const int64_t radius = size / 2;
  const int64_t r2     = radius * radius;

  constexpr int    S      = WorldChunkManager::SVO_SIZE;
  const glm::ivec3 origin = wcc * S;
  const glm::ivec3 center = origin + S / 2;

  auto session = m_ChunkManager->BeginWrite(wcc);

  m_ChunkManager->Set({size, size, size}, session, 2, 2, 2, light);

  for (int z = 0; z < size; z++)
    for (int y = 0; y < size; y++)
      for (int x = 0; x < size; x++) {
        const glm::ivec3 worldPos = origin + glm::ivec3(x, y, z);
        const glm::ivec3 d        = worldPos - center;

        const int64_t dist2 =
            int64_t(d.x) * d.x +
            int64_t(d.y) * d.y +
            int64_t(d.z) * d.z;

        if (dist2 <= r2)
          m_ChunkManager->Set(wcc, session, x, y, z, solid);
      }

  TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
}

const void World::GenerateCube(const uint32_t size, const glm::ivec3& wcc) {
  const auto light = m_Voxels.emplace_back(m_Palette.Find("Light")->Id);
  const auto solid = m_Voxels.emplace_back(m_Palette.Find("Grass Lush")->Id);

  m_ChunkManager->Set(wcc, 2, 2, 2, light);

  m_ChunkManager->Set(wcc, size, size, size, solid);

  TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
}

} // namespace vxen
