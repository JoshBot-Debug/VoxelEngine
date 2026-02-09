#include "World.h"

#include <execution>

#include "Application.h"
#include "Signal.h"
#include "Utility/Utility.h"
#include "Voxel/GreedyMesh64.h"

namespace Kitagawa {

World::World(uint32_t m_ChunkSize)
    : m_ChunkSize(m_ChunkSize) {
  m_ChunkManager = new ChunkManager(m_ChunkSize);

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

  Akari::Signal::Set(PALETTE_FLUSH_UPDATE);

  auto lightMaterial = m_Palette.Find("Light");
  auto light         = m_Voxels.emplace_back(std::make_shared<Voxel>(lightMaterial->Id));
  m_ChunkManager->Set(m_ChunkSize / 2, m_ChunkSize - 4, m_ChunkSize / 2, light.get());

  Akari::ThreadPool::Dispatch([&]() { GenerateCornellBox(); });
  // Akari::ThreadPool::Dispatch([&]() { GenerateHeightMapChunk({0, 0, 0}, 1.0f); });
}

World::~World() {
  delete m_ChunkManager;
}

void World::RenderUI() {
  m_Palette.RenderUI();

  ImGui::Begin("World", nullptr, ImGuiWindowFlags_MenuBar);

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("Options")) {

      if (ImGui::MenuItem("Flush"))
        Akari::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE | PALETTE_FLUSH_UPDATE);

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  ImGui::End();
}

void World::Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport) {
  if (Akari::Signal::Consume(CHUNK_MANAGER_SYNC_UPDATE))
    m_ChunkManager->Sync();

  glm::vec3 rayOrigin    = m_Camera->Position;
  glm::vec3 rayDirection = m_Camera->GetRayDirection(mouse.x, mouse.y);

  bool isCtrlPressed = ImGui::IsKeyPressed(ImGuiKey_LeftCtrl);
  bool isActing      = ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Left);

  if (isCtrlPressed && isActing) {
    SparseOctree<Voxel>::Hit hit = m_ChunkManager->DeepRaymarch(rayOrigin, rayDirection);

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
      m_ChunkManager->Clear(hit.Position);
      Akari::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      m_ChunkManager->Set(hit.Position + hit.Normal, hit.Data);
      Akari::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
    }
  }

  if (Akari::Signal::Consume(PALETTE_FLUSH_UPDATE)) {
    m_Materials            = m_Palette.GetMaterials();
    uint32_t maxMaterialId = 0;

    for (auto& mat : m_Materials)
      maxMaterialId = mat.Id > maxMaterialId ? mat.Id : maxMaterialId;

    m_MaterialsLUT.resize(maxMaterialId + 1);

    for (size_t i = 0; i < m_Materials.size(); i++)
      m_MaterialsLUT[m_Materials[i].Id] = i + 1;
  }

  if (Akari::Signal::Consume(CHUNK_MANAGER_FLUSH_UPDATE)) {
    uint64_t generation = m_ChunkManager->ReadLock();

    m_ChunkManager->Flatten(m_FlatSVO);

    m_ChunkManager->Filter(m_Lights, [this](const SparseOctree<Voxel>::Node* node) {
      auto material = m_Palette.GetMaterial(node->Data->Id);
      return material && material->Emissive.a > 0.0f;
    });

    m_ChunkManager->ReadUnlock(generation);

    m_ChunkManager->GreedyMesh(m_Palette.GetMaterials(), m_Vertices);
  }

  m_ChunkManager->Update(rayOrigin, rayDirection);
}

void World::Clean() {
  m_FlatSVO.clear();
  m_Materials.clear();
  m_Vertices.clear();
  m_Lights.clear();
}

const void World::GenerateHeightMapChunk(const glm::ivec3& origin, float step) {
  auto gLush   = m_Palette.Find("Grass Lush");
  auto gDry    = m_Palette.Find("Grass Dry");
  auto gForest = m_Palette.Find("Grass Forest");

  auto lush   = m_Voxels.emplace_back(std::make_shared<Voxel>(gLush->Id));
  auto dry    = m_Voxels.emplace_back(std::make_shared<Voxel>(gDry->Id));
  auto forest = m_Voxels.emplace_back(std::make_shared<Voxel>(gForest->Id));

  auto noise = m_HeightMap.Build(origin.x, origin.x + step, origin.z, origin.z + step);

  for (int z = 0; z < m_ChunkSize; z++)
    for (int x = 0; x < m_ChunkSize; x++) {
      float n      = noise.GetValue(x, z);
      int   height = static_cast<int>(std::round((std::clamp(n, -1.0f, 1.0f) + 1) * (m_ChunkSize / 2)));
      for (int y = 0; y < height; y++)
        m_ChunkManager->Set(x + (origin.x * m_ChunkSize), y + (origin.y * m_ChunkSize), z + (origin.z * m_ChunkSize), lush.get());
    }

  Akari::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
  // m_ChunkManager->Sync();
  // m_ChunkManager->Flush();
}

const void World::GenerateCornellBox() {
  auto leftWallMaterial  = m_Palette.Find("Left Wall");
  auto rightWallMaterial = m_Palette.Find("Right Wall");
  auto wallMaterial      = m_Palette.Find("Wall");
  auto cubeMaterial      = m_Palette.Find("Cube");
  auto sphereMaterial    = m_Palette.Find("Sphere");

  auto leftWall  = m_Voxels.emplace_back(std::make_shared<Voxel>(leftWallMaterial->Id));
  auto rightWall = m_Voxels.emplace_back(std::make_shared<Voxel>(rightWallMaterial->Id));
  auto wall      = m_Voxels.emplace_back(std::make_shared<Voxel>(wallMaterial->Id));
  auto cube      = m_Voxels.emplace_back(std::make_shared<Voxel>(cubeMaterial->Id));
  auto sphere    = m_Voxels.emplace_back(std::make_shared<Voxel>(sphereMaterial->Id));

  for (int a = 0; a < m_ChunkSize; a++) {
    for (int b = 0; b < m_ChunkSize; b++) {
      // Top wall
      m_ChunkManager->Set(a, m_ChunkSize - 1, b, wall.get());
      // Bottom wall
      m_ChunkManager->Set(a, 0, b, wall.get());
      // Left wall
      m_ChunkManager->Set(0, a, b, leftWall.get());
      // Right wall
      m_ChunkManager->Set(m_ChunkSize - 1, a, b, rightWall.get());
      // Back wall
      m_ChunkManager->Set(a, b, 0, wall.get());
      // Front wall
      m_ChunkManager->Set(a, b, m_ChunkSize - 1, wall.get());
    }
  }
  const glm::ivec2 blockDistanceFromWall = {m_ChunkSize / 4, m_ChunkSize / 6};
  const int        blockSize             = m_ChunkSize / 4;
  const int        blockHeight           = m_ChunkSize / 2;

  // Add the rectangle
  for (int z = 0; z < blockSize; z++)
    for (int x = 0; x < blockSize; x++)
      for (int y = 0; y < blockHeight; y++) {
        m_ChunkManager->Set(x + blockDistanceFromWall.x, y + 1, z + blockDistanceFromWall.y, cube.get());
      }

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
        if (dx * dx + dy * dy + dz * dz <= radius * radius) {
          m_ChunkManager->Set(x, y + 1, z, sphere.get());
        }
      }
    }
  }

  Akari::Signal::Set(CHUNK_MANAGER_FLUSH_UPDATE | CHUNK_MANAGER_SYNC_UPDATE);
  // m_ChunkManager->Sync();
  // m_ChunkManager->Flush();
}

} // namespace Kitagawa