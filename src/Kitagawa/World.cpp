#include "World.h"

#include <memory>

#include "Application.h"

namespace Kitagawa {

World::World(uint32_t chunkSize) : m_ChunkSize(chunkSize) {
  m_SVO = std::make_shared<SparseVoxelOctree>(m_ChunkSize);

  m_Palette.Create(Palette::Item{
      .Id = 0,
      .Name = "Left Wall",
      .Mat = std::make_shared<Material>(
          Material{.Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}}),
  });

  m_Palette.Create(Palette::Item{
      .Id = 0,
      .Name = "Right Wall",
      .Mat = std::make_shared<Material>(
          Material{.Albedo = glm::vec4{0.14f, 0.45f, 0.090f, 1.0f}}),
  });

  m_Palette.Create(Palette::Item{
      .Id = 0,
      .Name = "Wall",
      .Mat = std::make_shared<Material>(
          Material{.Albedo = glm::vec4{0.73f, 0.73f, 0.73f, 1.0f}}),
  });

  m_Palette.Create(Palette::Item{
      .Id = 0,
      .Name = "Cube",
      .Mat = std::make_shared<Material>(Material{.Albedo = glm::vec4(1.0f)}),
  });

  m_Palette.Create(Palette::Item{
      .Id = 0,
      .Name = "Sphere",
      .Mat = std::make_shared<Material>(Material{.Albedo = glm::vec4(1.0f)}),
  });

  m_Palette.Create(Palette::Item{
      .Id = 0,
      .Name = "Light",
      .Mat = std::make_shared<Material>(
          Material{.Albedo = glm::vec4(1.0f),
                   .Emissive = glm::vec4(1.0f, 1.0f, 1.0f, 3.0f)}),
  });

  GenerateCornellBox();
}

void World::RenderUI() { m_Palette.RenderUI(); }

const void World::GenerateCornellBox() {
  auto leftWallMaterial = m_Palette.Find("Left Wall");
  auto rightWallMaterial = m_Palette.Find("Right Wall");
  auto wallMaterial = m_Palette.Find("Wall");
  auto cubeMaterial = m_Palette.Find("Cube");
  auto sphereMaterial = m_Palette.Find("Sphere");
  auto lightMaterial = m_Palette.Find("Light");

  auto leftWall =
      m_Voxels.emplace_back(std::make_shared<Voxel>(leftWallMaterial->Id));
  auto rightWall =
      m_Voxels.emplace_back(std::make_shared<Voxel>(rightWallMaterial->Id));
  auto wall = m_Voxels.emplace_back(std::make_shared<Voxel>(wallMaterial->Id));
  auto cube = m_Voxels.emplace_back(std::make_shared<Voxel>(cubeMaterial->Id));
  auto sphere =
      m_Voxels.emplace_back(std::make_shared<Voxel>(sphereMaterial->Id));
  auto light =
      m_Voxels.emplace_back(std::make_shared<Voxel>(lightMaterial->Id));

  std::thread([chunkSize = m_ChunkSize, svo = m_SVO, wall, leftWall,
               rightWall]() {
    for (int a = 0; a < chunkSize; a++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(64));
      for (int b = 0; b < chunkSize; b++) {
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
  }).detach();

  const glm::vec2 blockDistanceFromWall =
      glm::vec2({m_ChunkSize / 4, m_ChunkSize / 6});
  const int blockSize = m_ChunkSize / 4;
  const int blockHeight = m_ChunkSize / 2;

  // Add the rectangle
  for (int z = 0; z < blockSize; z++)
    for (int x = 0; x < blockSize; x++)
      for (int y = 0; y < blockHeight; y++) {
        m_SVO->Set(x + blockDistanceFromWall.x, y + 1,
                   z + blockDistanceFromWall.y, cube.get());
      }

  // Add the sphere
  int radius = m_ChunkSize / 8;
  int cx = blockDistanceFromWall.x + (m_ChunkSize / 2);
  int cy = radius;
  int cz = blockDistanceFromWall.y + (m_ChunkSize / 2);

  for (int z = 0; z < m_ChunkSize; ++z) {
    for (int y = 0; y < m_ChunkSize; ++y) {
      for (int x = 0; x < m_ChunkSize; ++x) {
        float dx = x - cx;
        float dy = y - cy;
        float dz = z - cz;
        if (dx * dx + dy * dy + dz * dz <= radius * radius)
          m_SVO->Set(x, y + 1, z, sphere.get());
      }
    }
  }

  int lightSize = m_ChunkSize / 16;
  m_SVO->Set((m_ChunkSize / 2) - (lightSize / 2) - 1,
             m_ChunkSize - 1 - lightSize,
             (m_ChunkSize / 2) - (lightSize / 2) - 1, light.get(), lightSize);

  // glm::ivec3 lightBox =
  //     glm::ivec3(m_ChunkSize / 8, m_ChunkSize / 16, m_ChunkSize / 8);

  // int center = m_ChunkSize / 2;

  // for (int z = 0; z < lightBox.x; z += 2) {
  //   for (int y = 0; y < lightBox.y; y += 2) {
  //     for (int x = 0; x < lightBox.z; x += 2) {
  //       m_SVO->Set(x + (m_ChunkSize / 2) - (lightBox.x / 2),
  //                   y + m_ChunkSize - lightBox.y - 4,
  //                   z + (m_ChunkSize / 2) - (lightBox.z / 2), light.get(),
  //                   1);
  //     }
  //   }
  // }
}

void World::Flush() {
  m_Palette.Flush();
  /// Palette will flush tree but just for unifomity we flush anyways
  m_SVO->Flush();
};

} // namespace Kitagawa