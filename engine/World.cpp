#include "World.h"

#include <execution>

#include "Utility.h"
#include "thread/Signal.h"
#include "voxel/GreedyMesh64.h"
#include "window/Application.h"

namespace vxen {

inline glm::ivec3 getWorldChunkCoordinate(const glm::vec3& rayOrigin) {
  return {static_cast<int>(
              std::floor(rayOrigin.x / static_cast<float>(SparseOctree<Voxel>::SIZE))),
          static_cast<int>(
              std::floor(rayOrigin.y / static_cast<float>(SparseOctree<Voxel>::SIZE))),
          static_cast<int>(
              std::floor(rayOrigin.z / static_cast<float>(SparseOctree<Voxel>::SIZE)))};
};

inline glm::ivec3 getChunkLocalPosition(const glm::vec3& rayOrigin) {
  float size = static_cast<float>(SparseOctree<Voxel>::SIZE);

  float x = std::fmod(rayOrigin.x, size);
  float y = std::fmod(rayOrigin.y, size);
  float z = std::fmod(rayOrigin.z, size);

  return glm::ivec3(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z));
};

inline std::vector<glm::ivec3> getLocalChunkCoordinates(const glm::vec3& coord) {
  std::vector<glm::ivec3> result;
  // for (int dz = -ChunkManager::CHUNK_RADIUS.z; dz <= ChunkManager::CHUNK_RADIUS.z; dz++)
  //   for (int dx = -ChunkManager::CHUNK_RADIUS.x; dx <= ChunkManager::CHUNK_RADIUS.x; dx++)
  //     for (int dy = -ChunkManager::CHUNK_RADIUS.y; dy <= ChunkManager::CHUNK_RADIUS.y; dy++)
  //       result.emplace_back(coord.x + dx, coord.y + dy, coord.z + dz);
  return result;
};

World::World(uint32_t m_ChunkSize)
    : m_ChunkSize(m_ChunkSize) {
  // m_ChunkManager = new ChunkManager(m_ChunkSize);

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

  // akari::thread::ThreadPool::Dispatch([&]() { GenerateCornellBox(); });
  // akari::thread::ThreadPool::Dispatch([&]() { GenerateChunk({0, 0, 0}, 1.0f); });
  // GenerateChunk({0, 0, 0});
}

World::~World() {
  // delete m_ChunkManager;
}

void World::RenderUI() {
  m_Palette.RenderUI();
}

void World::Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport) {

  // glm::vec3 rayOrigin    = m_Camera->Position;
  // glm::vec3 rayDirection = m_Camera->GetRayDirection(mouse.x, mouse.y);

  // // glm::ivec3 wcc = getWorldChunkCoordinate(rayOrigin);
  // glm::ivec3 wcc = {0, 0, 0};

  // /// TODO: We are syncing here, only one chunk?? Need to sync all dirty chunks :|
  // if (akari::thread::Signal::Consume(CHUNK_MANAGER_SYNC_UPDATE)) {
  //   std::vector<glm::ivec3> lccs = getLocalChunkCoordinates(wcc);
  //   for (auto& lcc : lccs)
  //     m_ChunkManager->Sync(lcc);
  // }

  // bool isCtrlPressed = ImGui::IsKeyPressed(ImGuiKey_LeftCtrl);
  // bool isActing      = ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Left);

  // if (ImGui::IsKeyPressed(ImGuiKey_F5))
  //   akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE | PALETTE_FLUSH_UPDATE);

  // if (ImGui::IsKeyPressed(ImGuiKey_T)) {
  //   SparseOctree<Voxel>::Hit hit      = m_ChunkManager->DeepRaymarch(wcc, rayOrigin, rayDirection);
  //   auto                     position = getChunkLocalPosition(hit.Position + hit.Normal);
  //   m_ChunkManager->Set(wcc, position, hit.Data);
  //   akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
  // }

  // if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
  //   SparseOctree<Voxel>::Hit hit      = m_ChunkManager->DeepRaymarch(wcc, rayOrigin, rayDirection);
  //   auto                     position = getChunkLocalPosition(hit.Position);
  //   m_ChunkManager->Clear(wcc, position);
  //   akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
  // }

  // if (isCtrlPressed && isActing) {
  //   SparseOctree<Voxel>::Hit hit = m_ChunkManager->DeepRaymarch(wcc, rayOrigin, rayDirection);

  //   if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
  //     auto position = getChunkLocalPosition(hit.Position);
  //     m_ChunkManager->Clear(wcc, position);
  //     akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
  //   }

  //   if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
  //     auto position = getChunkLocalPosition(hit.Position + hit.Normal);
  //     m_ChunkManager->Set(wcc, position, hit.Data);
  //     akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
  //   }
  // }

  // if (akari::thread::Signal::Consume(PALETTE_FLUSH_UPDATE)) {
  //   m_Materials            = m_Palette.GetMaterials();
  //   uint32_t maxMaterialId = 0;

  //   for (auto& mat : m_Materials)
  //     maxMaterialId = mat.Id > maxMaterialId ? mat.Id : maxMaterialId;

  //   m_MaterialsLUT.resize(maxMaterialId + 1);

  //   for (size_t i = 0; i < m_Materials.size(); i++)
  //     m_MaterialsLUT[m_Materials[i].Id] = i + 1;
  // }

  // if (akari::thread::Signal::Consume(CHUNK_MANAGER_FLUSH_UPDATE)) {
  //   {
  //     auto session = m_ChunkManager->BeginRead(wcc);

  //     m_ChunkManager->Flatten(session, wcc, m_FlatSVO);

  //     m_ChunkManager->Filter(session, wcc, m_Lights, [this](const SparseOctree<Voxel>::Node* node) {
  //       auto material = m_Palette.GetMaterial(node->Data->Id);
  //       return material && material->Emissive.a > 0.0f;
  //     });
  //   }

  //   std::vector<glm::ivec3> lccs = getLocalChunkCoordinates(wcc);

  //   for (auto& lcc : lccs)
  //     m_ChunkManager->GreedyMesh(getWorldChunkCoordinate(rayOrigin), lcc, m_Palette.GetMaterials(), m_Vertices);
  // }

  // if (m_CurrentOrigin != getWorldChunkCoordinate(rayOrigin)) {
  //   GenerateChunk(wcc);
  //   m_CurrentOrigin = getWorldChunkCoordinate(rayOrigin);
  // }
}

void World::Clean() {
  m_FlatSVO.clear();
  m_Materials.clear();
  // m_Vertices.clear();
  m_Lights.clear();
}

const void World::GenerateChunk(const glm::ivec3& wcc) {
  // auto gLush         = m_Palette.Find("Grass Lush");
  // auto lightMaterial = m_Palette.Find("Light");

  // auto lush  = m_Voxels.emplace_back(std::make_shared<Voxel>(gLush->Id));
  // auto light = m_Voxels.emplace_back(std::make_shared<Voxel>(lightMaterial->Id));

  // std::vector<glm::ivec3> lccs = getLocalChunkCoordinates(wcc);

  // for (auto& lcc : lccs)
  //   if (lcc.y == 0) {
  //     auto session = m_ChunkManager->BeginWrite(lcc);

  //     auto noise = m_HeightMap.Build(lcc.x, lcc.x + 1.0f, lcc.z, lcc.z + 1.0f);

  //     session.Root->Destroy();

  //     for (int z = 0; z < m_ChunkSize; z++)
  //       for (int x = 0; x < m_ChunkSize; x++) {
  //         float n      = noise.GetValue(x, z);
  //         int   height = static_cast<int>(std::round((std::clamp(n, -1.0f, 1.0f) + 1) * (m_ChunkSize / 2)));
  //         for (int y = 0; y < height; y++)
  //           m_ChunkManager->Set(lcc, session, x, y, z, lush.get());
  //       }
  //   }

  // m_ChunkManager->Set({0, 0, 0}, m_ChunkSize / 2, m_ChunkSize - 4, m_ChunkSize / 2, light.get());

  // akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
}

const void World::GenerateCornellBox() {
  // auto leftWallMaterial  = m_Palette.Find("Left Wall");
  // auto rightWallMaterial = m_Palette.Find("Right Wall");
  // auto wallMaterial      = m_Palette.Find("Wall");
  // auto cubeMaterial      = m_Palette.Find("Cube");
  // auto sphereMaterial    = m_Palette.Find("Sphere");
  // auto lightMaterial     = m_Palette.Find("Light");

  // auto leftWall  = m_Voxels.emplace_back(std::make_shared<Voxel>(leftWallMaterial->Id));
  // auto rightWall = m_Voxels.emplace_back(std::make_shared<Voxel>(rightWallMaterial->Id));
  // auto wall      = m_Voxels.emplace_back(std::make_shared<Voxel>(wallMaterial->Id));
  // auto cube      = m_Voxels.emplace_back(std::make_shared<Voxel>(cubeMaterial->Id));
  // auto sphere    = m_Voxels.emplace_back(std::make_shared<Voxel>(sphereMaterial->Id));
  // auto light     = m_Voxels.emplace_back(std::make_shared<Voxel>(lightMaterial->Id));

  // m_ChunkManager->Set({0, 0, 0}, m_ChunkSize / 2, m_ChunkSize - 4, m_ChunkSize / 2, light.get());

  // auto origin = glm::ivec3(0);

  // {
  //   auto guard = m_ChunkManager->BeginWrite(origin);

  //   for (int a = 0; a < m_ChunkSize; a++) {
  //     for (int b = 0; b < m_ChunkSize; b++) {
  //       // Top wall
  //       m_ChunkManager->Set(origin, guard, a, m_ChunkSize - 1, b, wall.get());
  //       // Bottom wall
  //       m_ChunkManager->Set(origin, guard, a, 0, b, wall.get());
  //       // Left wall
  //       m_ChunkManager->Set(origin, guard, 0, a, b, leftWall.get());
  //       // Right wall
  //       m_ChunkManager->Set(origin, guard, m_ChunkSize - 1, a, b, rightWall.get());
  //       // Back wall
  //       m_ChunkManager->Set(origin, guard, a, b, 0, wall.get());
  //       // Front wall
  //       // m_ChunkManager->Set(origin, guard, a, b, m_ChunkSize - 1, wall.get());
  //     }
  //   }
  //   const glm::ivec2 blockDistanceFromWall = {m_ChunkSize / 4, m_ChunkSize / 6};
  //   const int        blockSize             = m_ChunkSize / 4;
  //   const int        blockHeight           = m_ChunkSize / 2;

  //   // Add the rectangle
  //   for (int z = 0; z < blockSize; z++)
  //     for (int x = 0; x < blockSize; x++)
  //       for (int y = 0; y < blockHeight; y++)
  //         m_ChunkManager->Set(origin, guard, x + blockDistanceFromWall.x, y + 1, z + blockDistanceFromWall.y, cube.get());

  //   // Add the sphere
  //   int radius = m_ChunkSize / 8;
  //   int cx     = blockDistanceFromWall.x + (m_ChunkSize / 2);
  //   int cy     = radius;
  //   int cz     = blockDistanceFromWall.y + (m_ChunkSize / 2);

  //   for (int z = 0; z < m_ChunkSize; ++z) {
  //     for (int y = 0; y < m_ChunkSize; ++y) {
  //       for (int x = 0; x < m_ChunkSize; ++x) {
  //         float dx = x - cx;
  //         float dy = y - cy;
  //         float dz = z - cz;
  //         if (dx * dx + dy * dy + dz * dz <= radius * radius)
  //           m_ChunkManager->Set(origin, guard, x, y + 1, z, sphere.get());
  //       }
  //     }
  //   }
  // }

  // akari::thread::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
}

} // namespace vxen