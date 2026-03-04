#include "World.h"

#include "Utility.h"
#include "thread/Signal.h"
#include "thread/ThreadPool.h"
#include "voxel/GreedyMesh64.h"
#include "window/Application.h"

namespace vxen {

World::World(uint32_t m_ChunkSize)
    : m_ChunkSize(m_ChunkSize) {
  m_ChunkManager = new WorldChunkManager();

  m_HeightMap.Initialize(m_ChunkSize, m_ChunkSize);

  m_Palette.Create(Palette::Item{
      .Name = "Left Wall",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}}),
  });

  m_Palette.Create(Palette::Item{
      .Name = "Right Wall",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4{0.14f, 0.45f, 0.090f, 1.0f}}),
  });

  m_Palette.Create(Palette::Item{
      .Name = "Wall",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4{0.73f, 0.73f, 0.73f, 1.0f}}),
  });

  m_Palette.Create(Palette::Item{
      .Name = "Cube",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4(1.0f)}),
  });

  m_Palette.Create(Palette::Item{
      .Name = "Sphere",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4(1.0f)}),
  });

  m_Palette.Create(Palette::Item{
      .Name = "Grass Lush",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4(0.30f, 0.60f, 0.25f, 1.0f)}),
  });

  m_Palette.Create(Palette::Item{
      .Name = "Grass Dry",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4(0.55f, 0.60f, 0.30f, 1.0f)}),
  });

  m_Palette.Create(Palette::Item{
      .Name = "Grass Forest",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4(0.22f, 0.42f, 0.18f, 1.0f)}),
  });

  m_Palette.Create(Palette::Item{
      .Name = "Light",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4(1.0f), .Emissive = glm::vec4(1.0f, 1.0f, 1.0f, 3.0f)}),
  });

  akari::thread::Signal::Set(PALETTE_FLUSH_UPDATE);

  akari::thread::ThreadPool::Dispatch([&]() { GenerateCornellBox({0, 0, 0}); });
  // akari::thread::ThreadPool::Dispatch([&]() { GenerateChunk({0, 0, 0}, 1.0f); });

  // for (size_t z = 0; z < WorldChunkManager::CHUNK_SIZE; z++)
  //   for (size_t y = 0; y < WorldChunkManager::CHUNK_SIZE; y++)
  //     for (size_t x = 0; x < WorldChunkManager::CHUNK_SIZE; x++)
  //       GenerateNoiseSphere({x, y, z});
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

  /// TODO: We are syncing here, only one chunk?? Need to sync all dirty chunks :|
  if (akari::thread::Signal::Consume(CHUNK_MANAGER_SYNC_UPDATE))
    m_ChunkManager->Sync(wcc);

  bool isCtrlPressed = ImGui::IsKeyPressed(ImGuiKey_LeftCtrl);
  bool isActing      = ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Left);

  if (ImGui::IsKeyPressed(ImGuiKey_F5))
    akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE | PALETTE_FLUSH_UPDATE);

  if (ImGui::IsKeyPressed(ImGuiKey_T)) {
    SparseOctree<Voxel>::Hit hit      = m_ChunkManager->DeepRaymarch(wcc, rayOrigin, rayDirection);
    auto                     position = m_ChunkManager->WorldToLocalCoordinate(hit.Position + hit.Normal);
    m_ChunkManager->Set(wcc, position, hit.Data);
    akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
    SparseOctree<Voxel>::Hit hit      = m_ChunkManager->DeepRaymarch(wcc, rayOrigin, rayDirection);
    auto                     position = m_ChunkManager->WorldToLocalCoordinate(hit.Position);
    m_ChunkManager->Clear(wcc, position);
    akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
  }

  if (isCtrlPressed && isActing) {
    SparseOctree<Voxel>::Hit hit = m_ChunkManager->DeepRaymarch(wcc, rayOrigin, rayDirection);

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
      auto position = m_ChunkManager->WorldToLocalCoordinate(hit.Position);
      m_ChunkManager->Clear(wcc, position);
      akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      auto position = m_ChunkManager->WorldToLocalCoordinate(hit.Position + hit.Normal);
      m_ChunkManager->Set(wcc, position, hit.Data);
      akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
    }
  }

  if (akari::thread::Signal::Consume(PALETTE_FLUSH_UPDATE)) {
    m_Materials            = m_Palette.GetMaterials();
    uint32_t maxMaterialId = 0;

    for (auto& mat : m_Materials)
      maxMaterialId = mat.Id > maxMaterialId ? mat.Id : maxMaterialId;

    m_MaterialsLUT.resize(maxMaterialId + 1);

    for (size_t i = 0; i < m_Materials.size(); i++)
      m_MaterialsLUT[m_Materials[i].Id] = i + 1;
  }

  if (akari::thread::Signal::Consume(CHUNK_MANAGER_FLUSH_UPDATE)) {
    {
      auto session = m_ChunkManager->BeginRead(wcc);

      m_FlatSVO = m_ChunkManager->Flatten(wcc, session);

      m_ChunkManager->Filter(wcc, session, m_Lights, [this](const SparseOctree<Voxel>::Node* node) {
        auto material = m_Palette.GetMaterial(node->Data->Id);
        return material && material->Emissive.a > 0.0f;
      });
    }

    std::vector<uint32_t> ids;

    for (auto& material : m_Palette.GetMaterials())
      ids.push_back(material.Id);

    // for (size_t z = 0; z < 4; z++)
    //   for (size_t y = 0; y < 4; y++)
    //     for (size_t x = 0; x < 4; x++) {
    for (size_t z = 0; z < 1; z++)
      for (size_t y = 0; y < 1; y++)
        for (size_t x = 0; x < 1; x++) {
          auto chunkVertices = m_ChunkManager->GreedyMesh({x, y, z}, ids);
          m_Vertices.insert(m_Vertices.end(), chunkVertices.begin(), chunkVertices.end());
        }

    akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_RENDER);
  }
}

void World::Clean() {
  m_FlatSVO.clear();
  m_Materials.clear();
  m_Vertices.clear();
  m_Lights.clear();
}

const void World::GenerateChunk(const glm::ivec3& wcc) {
  auto gLush         = m_Palette.Find("Grass Lush");
  auto lightMaterial = m_Palette.Find("Light");

  auto lush  = m_Voxels.emplace_back(std::make_shared<Voxel>(gLush->Id));
  auto light = m_Voxels.emplace_back(std::make_shared<Voxel>(lightMaterial->Id));

  {
    auto session = m_ChunkManager->BeginWrite(wcc);

    auto noise = m_HeightMap.Build(wcc.x, wcc.x + 1.0f, wcc.z, wcc.z + 1.0f);

    session.Root->Destroy();

    for (int z = 0; z < m_ChunkSize; z++)
      for (int x = 0; x < m_ChunkSize; x++) {
        float n      = noise.GetValue(x, z);
        int   height = static_cast<int>(std::round((std::clamp(n, -1.0f, 1.0f) + 1) * (m_ChunkSize / 2)));
        for (int y = 0; y < height; y++)
          m_ChunkManager->Set(wcc, session, x, y, z, lush.get());
      }

    m_ChunkManager->Set(wcc, session, m_ChunkSize / 2, m_ChunkSize - 4, m_ChunkSize / 2, light.get());
  }

  akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
}

const void World::GenerateCornellBox(const glm::u8vec3& origin) {
  auto leftWallMaterial  = m_Palette.Find("Left Wall");
  auto rightWallMaterial = m_Palette.Find("Right Wall");
  auto wallMaterial      = m_Palette.Find("Wall");
  auto cubeMaterial      = m_Palette.Find("Cube");
  auto sphereMaterial    = m_Palette.Find("Sphere");
  auto lightMaterial     = m_Palette.Find("Light");

  auto leftWall  = m_Voxels.emplace_back(std::make_shared<Voxel>(leftWallMaterial->Id));
  auto rightWall = m_Voxels.emplace_back(std::make_shared<Voxel>(rightWallMaterial->Id));
  auto wall      = m_Voxels.emplace_back(std::make_shared<Voxel>(wallMaterial->Id));
  auto cube      = m_Voxels.emplace_back(std::make_shared<Voxel>(cubeMaterial->Id));
  auto sphere    = m_Voxels.emplace_back(std::make_shared<Voxel>(sphereMaterial->Id));
  auto light     = m_Voxels.emplace_back(std::make_shared<Voxel>(lightMaterial->Id));

  {
    auto session = m_ChunkManager->BeginWrite(origin);

    m_ChunkManager->Set(origin, session, m_ChunkSize / 2, m_ChunkSize - 4, m_ChunkSize / 2, light.get());

    for (int a = 0; a < m_ChunkSize; a++) {
      for (int b = 0; b < m_ChunkSize; b++) {
        // Top wall
        m_ChunkManager->Set(origin, session, a, m_ChunkSize - 1, b, wall.get());
        // Bottom wall
        m_ChunkManager->Set(origin, session, a, 0, b, wall.get());
        // Left wall
        m_ChunkManager->Set(origin, session, 0, a, b, leftWall.get());
        // Right wall
        m_ChunkManager->Set(origin, session, m_ChunkSize - 1, a, b, rightWall.get());
        // Back wall
        m_ChunkManager->Set(origin, session, a, b, 0, wall.get());
        // Front wall
        // m_ChunkManager->Set(origin, session, a, b, m_ChunkSize - 1, wall.get());
      }
    }
    const glm::ivec2 blockDistanceFromWall = {m_ChunkSize / 4, m_ChunkSize / 6};
    const int        blockSize             = m_ChunkSize / 4;
    const int        blockHeight           = m_ChunkSize / 2;

    // Add the rectangle
    for (int z = 0; z < blockSize; z++)
      for (int x = 0; x < blockSize; x++)
        for (int y = 0; y < blockHeight; y++)
          m_ChunkManager->Set(origin, session, x + blockDistanceFromWall.x, y + 1, z + blockDistanceFromWall.y, cube.get());

    // Add the sphere
    int radius = m_ChunkSize / 8;
    int cx     = blockDistanceFromWall.x + (m_ChunkSize / 2);
    int cy     = radius;
    int cz     = blockDistanceFromWall.y + (m_ChunkSize / 2);

    for (int z = 0; z < m_ChunkSize; ++z) {
      for (int y = 0; y < m_ChunkSize; ++y) {
        for (int x = 0; x < m_ChunkSize; ++x) {
          float dx = x - cx;
          float dy = y - cy;
          float dz = z - cz;
          if (dx * dx + dy * dy + dz * dz <= radius * radius)
            m_ChunkManager->Set(origin, session, x, y + 1, z, sphere.get());
        }
      }
    }

    m_ChunkManager->Set(origin, session, 16, 0, 48, wall.get());
    m_ChunkManager->Set(origin, session, 16, 1, 48, wall.get());
    m_ChunkManager->Set(origin, session, 16, 2, 48, leftWall.get());
    m_ChunkManager->Set(origin, session, 16, 3, 48, leftWall.get());
    m_ChunkManager->Set(origin, session, 16, 4, 48, rightWall.get());
    m_ChunkManager->Set(origin, session, 16, 5, 48, rightWall.get());
  }

  akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
}

const void World::GenerateSphere(const glm::ivec3& wcc) {
  auto solid = m_Voxels.emplace_back(std::make_shared<Voxel>(m_Palette.Find("Grass Lush")->Id));
  auto light = m_Voxels.emplace_back(std::make_shared<Voxel>(m_Palette.Find("Light")->Id));

  {
    auto session = m_ChunkManager->BeginWrite(wcc);

    const glm::ivec3 center = glm::ivec3((WorldChunkManager::CHUNK_SIZE >> 1) * WorldChunkManager::SVO_SIZE);
    const int        radius = ((WorldChunkManager::CHUNK_SIZE >> 1) * WorldChunkManager::SVO_SIZE) - (WorldChunkManager::SVO_SIZE >> 1);
    const int        r2     = radius * radius;

    glm::ivec3 chunkOrigin = wcc * (int)WorldChunkManager::SVO_SIZE;

    for (int z = 0; z < WorldChunkManager::SVO_SIZE; z++)
      for (int y = 0; y < WorldChunkManager::SVO_SIZE; y++)
        for (int x = 0; x < WorldChunkManager::SVO_SIZE; x++) {
          glm::ivec3 worldPos = chunkOrigin + glm::ivec3(x, y, z);

          glm::ivec3 d     = worldPos - center;
          int        dist2 = d.x * d.x + d.y * d.y + d.z * d.z;

          if (dist2 <= r2)
            m_ChunkManager->Set(wcc, session, x, y, z, solid.get());
        }
  }

  akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
}

const void World::GenerateNoiseSphere(const glm::ivec3& wcc) {
  auto solid = m_Voxels.emplace_back(std::make_shared<Voxel>(m_Palette.Find("Grass Lush")->Id));
  auto light = m_Voxels.emplace_back(std::make_shared<Voxel>(m_Palette.Find("Light")->Id));
  {
    auto session = m_ChunkManager->BeginWrite(wcc);

    const glm::ivec3 center      = glm::ivec3((WorldChunkManager::CHUNK_SIZE >> 1) * WorldChunkManager::SVO_SIZE);
    const int        radius      = ((WorldChunkManager::CHUNK_SIZE >> 1) * WorldChunkManager::SVO_SIZE) - (WorldChunkManager::SVO_SIZE >> 1);
    const int        heightScale = m_ChunkSize / 2;

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
            m_ChunkManager->Set(wcc, session, x, y, z, solid.get());
          }
        }
  }

  akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
}

} // namespace vxen