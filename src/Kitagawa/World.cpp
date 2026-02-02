#include "World.h"

#include <execution>

#include "Application.h"
#include "Utility/Utility.h"
#include "Voxel/GreedyMesh64.h"

namespace Kitagawa {

World::World(uint32_t chunkSize)
    : m_ChunkSize(chunkSize) {
  m_ChunkManager = new ChunkManager(chunkSize);

  m_HeightMap.Initialize(chunkSize, chunkSize);

  m_Palette.OnFlush([this]() {
    m_Materials            = m_Palette.GetMaterials();
    uint32_t maxMaterialId = 0;

    for (auto& mat : m_Materials)
      maxMaterialId = mat.Id > maxMaterialId ? mat.Id : maxMaterialId;

    m_MaterialsLUT.resize(maxMaterialId + 1);

    for (size_t i = 0; i < m_Materials.size(); i++)
      m_MaterialsLUT[m_Materials[i].Id] = i + 1;
  });

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

  m_Palette.Flush();

  auto lightMaterial = m_Palette.Find("Light");
  auto light         = m_Voxels.emplace_back(std::make_shared<Voxel>(lightMaterial->Id));

  int lightSize = m_ChunkSize / 16;
  int center    = (m_ChunkSize / 2) - (lightSize / 2) - 1;
  m_ChunkManager->Set(center, m_ChunkSize - 1 - lightSize, center, light.get(), lightSize);

  GenerateCornellBox();
  // GenerateHeightMapChunk({0, 0, 0}, 1.0f);
}

World::~World() {
  delete m_ChunkManager;
}

void World::RenderUI() { m_Palette.RenderUI(); }

void World::Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport) {
  glm::vec3 rayOrigin    = m_Camera->Position;
  glm::vec3 rayDirection = m_Camera->GetRayDirection(mouse.x, mouse.y);

  bool isCtrlPressed = ImGui::IsKeyPressed(ImGuiKey_LeftCtrl);
  bool isActing      = ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Left);

  if (isCtrlPressed && isActing) {
    SparseOctree<Voxel>::Hit hit = m_ChunkManager->DeepRaymarch(rayOrigin, rayDirection);

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
      m_ChunkManager->Clear(hit.Position, hit.Size);

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
      m_ChunkManager->Set(hit.Position + hit.Normal, hit.Data, hit.Size);

    if (m_ChunkManager->IsDirty())
      m_ChunkManager->Flush();
  }

  if (m_ChunkManager->IsDirty()) {

    m_ChunkManager->Flatten(m_FlatSVO);

    m_ChunkManager->Filter(m_Lights, [this](const SparseOctree<Voxel>::Node* node) {
      auto material = m_Palette.GetMaterial(node->Data->Id);
      return material && material->Emissive.a > 0.0f;
    });

    m_ChunkManager->GreedyMesh(m_Palette.GetMaterials(), m_Vertices);

    m_ChunkManager->Update(rayOrigin, rayDirection);
  }
}

bool World::IsDirty() {
  return m_Palette.IsDirty() || m_ChunkManager->IsDirty();
}

void World::Clean() {
  m_Palette.Clean();
  m_ChunkManager->Clean();

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

  uint64_t mask[(m_ChunkSize * m_ChunkSize) * (m_ChunkSize / 64)] = {0};

  for (int z = 0; z < m_ChunkSize; z++)
    for (int x = 0; x < m_ChunkSize; x++) {
      float n      = noise.GetValue(x, z);
      int   height = static_cast<int>(std::round((std::clamp(n, -1.0f, 1.0f) + 1) * (m_ChunkSize / 2)));
      for (int y = 0; y < height; y++) {
        int index = x + m_ChunkSize * (z + m_ChunkSize * y);
        mask[index / 64] |= 1ULL << (index % 64);
      }
    }

  m_ChunkManager->Set(mask, lush.get());
  m_ChunkManager->Flush();
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

  std::thread([chunkSize = m_ChunkSize, svo = m_ChunkManager, sphere, cube, wall, leftWall, rightWall]() {
    for (int a = 0; a < chunkSize; a++) {
      for (int b = 0; b < chunkSize; b++) {
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // Top wall
        svo->Set(a, chunkSize - 1, b, wall.get());
        // Bottom wall
        svo->Set(a, 0, b, wall.get());
        // Left wall
        svo->Set(0, a, b, leftWall.get());
        // Right wall
        svo->Set(chunkSize - 1, a, b, rightWall.get());
        // Back wall
        svo->Set(a, b, 0, wall.get());
        // Front wall
        svo->Set(a, b, chunkSize - 1, wall.get());
      }
    }
    const glm::ivec2 blockDistanceFromWall = {chunkSize / 4, chunkSize / 6};
    const int        blockSize             = chunkSize / 4;
    const int        blockHeight           = chunkSize / 2;

    // Add the rectangle
    for (int z = 0; z < blockSize; z++)
      for (int x = 0; x < blockSize; x++)
        for (int y = 0; y < blockHeight; y++) {
          // std::this_thread::sleep_for(std::chrono::milliseconds(1));
          svo->Set(x + blockDistanceFromWall.x, y + 1, z + blockDistanceFromWall.y, cube.get());
        }

    // Add the sphere
    int radius = chunkSize / 8;
    int cx     = blockDistanceFromWall.x + (chunkSize / 2);
    int cy     = radius;
    int cz     = blockDistanceFromWall.y + (chunkSize / 2);

    for (int z = 0; z < chunkSize; ++z) {
      for (int y = 0; y < chunkSize; ++y) {
        for (int x = 0; x < chunkSize; ++x) {
          float dx = x - cx;
          float dy = y - cy;
          float dz = z - cz;
          if (dx * dx + dy * dy + dz * dz <= radius * radius) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            svo->Set(x, y + 1, z, sphere.get());
          }
        }
      }
    }
  }).detach();
}

} // namespace Kitagawa