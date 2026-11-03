#include "World.h"
#include "thread/ThreadPool.h"

#include <imgui.h>

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

  ThreadPool::Dispatch([&]() { GenerateCornellBox(64, {0, 0, 0}); });
  // ThreadPool::Dispatch([&]() { GenerateSphere(64, {0, 0, 0}); });
  // ThreadPool::Dispatch([&]() { GenerateCube(1, {0, 0, 0}); });

  // ThreadPool::Dispatch([&]() {
  //   // for (size_t z = 0; z < WorldChunkManager::CHUNK_SIZE; z++)
  //   //   for (size_t x = 0; x < WorldChunkManager::CHUNK_SIZE; x++)
  //   //     GenerateChunk({x, 0, z});
  //   for (size_t z = 0; z < 1; z++)
  //     for (size_t x = 0; x < 2; x++)
  //       GenerateChunk({x, 0, z});
  //   TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
  // });
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
    SparseOctree::Hit hit      = m_ChunkManager->DeepRaymarch(wcc, rayOrigin, rayDirection);
    auto              position = m_ChunkManager->WorldToLocalCoordinate(hit.Position + hit.Normal);
    m_ChunkManager->Set(wcc, position, hit.Id);
    TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
    SparseOctree::Hit hit      = m_ChunkManager->DeepRaymarch(wcc, rayOrigin, rayDirection);
    auto              position = m_ChunkManager->WorldToLocalCoordinate(hit.Position);
    m_ChunkManager->Clear(wcc, position);
    TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
  }

  if (isCtrlPressed && isActing) {
    SparseOctree::Hit hit = m_ChunkManager->DeepRaymarch(wcc, rayOrigin, rayDirection);

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

const void World::GenerateCornellBox(const uint32_t size, const glm::u8vec3& origin) {
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

  {
    auto session = m_ChunkManager->BeginWrite(origin);

    m_ChunkManager->Set(origin, session, size / 2, size - 4, size / 2, light);

    for (int a = 0; a < size; a++) {
      for (int b = 0; b < size; b++) {
        // Top wall
        m_ChunkManager->Set(origin, session, a, size - 1, b, wall);
        // Bottom wall
        m_ChunkManager->Set(origin, session, a, 0, b, wall);
        // Left wall
        m_ChunkManager->Set(origin, session, 0, a, b, leftWall);
        // Right wall
        m_ChunkManager->Set(origin, session, size - 1, a, b, rightWall);
        // Back wall
        m_ChunkManager->Set(origin, session, a, b, 0, wall);
        // Front wall
        // m_ChunkManager->Set(origin, session, a, b, size - 1, wall);
      }
    }
    const glm::ivec2 blockDistanceFromWall = {size / 4, size / 6};
    const int        blockSize             = size / 4;
    const int        blockHeight           = size / 2;

    // Add the rectangle
    for (int z = 0; z < blockSize; z++)
      for (int x = 0; x < blockSize; x++)
        for (int y = 0; y < blockHeight; y++)
          m_ChunkManager->Set(origin, session, x + blockDistanceFromWall.x, y + 1, z + blockDistanceFromWall.y, cube);

    // Add the sphere
    int radius = size / 8;
    int cx     = blockDistanceFromWall.x + (size / 2);
    int cy     = radius;
    int cz     = blockDistanceFromWall.y + (size / 2);

    for (int z = 0; z < size; ++z) {
      for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
          float dx = x - cx;
          float dy = y - cy;
          float dz = z - cz;
          if (dx * dx + dy * dy + dz * dz <= radius * radius)
            m_ChunkManager->Set(origin, session, x, y + 1, z, sphere);
        }
      }
    }

    m_ChunkManager->Set(origin, session, 16, 0, 48, wall);
    m_ChunkManager->Set(origin, session, 16, 1, 48, wall);
    m_ChunkManager->Set(origin, session, 16, 2, 48, leftWall);
    m_ChunkManager->Set(origin, session, 16, 3, 48, leftWall);
    m_ChunkManager->Set(origin, session, 16, 4, 48, rightWall);
    m_ChunkManager->Set(origin, session, 16, 5, 48, rightWall);
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

const void World::GenerateNoiseSphere(const uint32_t size, const glm::ivec3& wcc) {
  auto solid = m_Voxels.emplace_back(m_Palette.Find("Grass Lush")->Id);
  auto light = m_Voxels.emplace_back(m_Palette.Find("Light")->Id);
  {
    auto session = m_ChunkManager->BeginWrite(wcc);

    const glm::ivec3 center      = glm::ivec3((WorldChunkManager::CHUNK_SIZE >> 1) * WorldChunkManager::SVO_SIZE);
    const int        radius      = ((WorldChunkManager::CHUNK_SIZE >> 1) * WorldChunkManager::SVO_SIZE) - (WorldChunkManager::SVO_SIZE >> 1);
    const int        heightScale = size / 2;

    glm::ivec3 min = wcc * (int)WorldChunkManager::SVO_SIZE;

    auto noise = m_HeightMap.Build(wcc.x, wcc.x + 1.0f, wcc.z, wcc.z + 1.0f);

    for (int z = 0; z < WorldChunkManager::SVO_SIZE; z++)
      for (int y = 0; y < WorldChunkManager::SVO_SIZE; y++)
        for (int x = 0; x < WorldChunkManager::SVO_SIZE; x++) {
          glm::ivec3 worldPos = min + glm::ivec3(x, y, z);

          glm::ivec3 d    = worldPos - center;
          float      dist = glm::length(glm::vec3(d));

          // Direction on sphere
          glm::vec3 dir = glm::normalize(glm::vec3(d));

          // Sample noise using spherical projection
          float n = noise.GetValue(dir.x, dir.y); // simple version

          float surfaceRadius = radius + n * heightScale;

          if (dist <= surfaceRadius) {
            m_ChunkManager->Set(wcc, session, x, y, z, solid);
          }
        }
  }

  TSignal::Set(0, CHUNK_MANAGER_FLUSH_UPDATE);
}

} // namespace vxen
