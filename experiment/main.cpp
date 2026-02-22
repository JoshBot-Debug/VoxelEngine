#include <iostream>
#include <vector>
#include <math.h>

#include "Debug.h"
#include "voxel/ChunkManager.h"
#include "voxel/GreedyMesh64.h"
#include "voxel/HeightMap.h"
#include "voxel/Palette.h"
#include "voxel/SparseOctree.h"
#include "voxel/Voxel.h"

inline std::vector<glm::ivec3> getLocalChunkCoordinates(const glm::vec3& coord) {
  std::vector<glm::ivec3> result;
  // for (int dz = -ChunkManager::CHUNK_RADIUS.z; dz <= ChunkManager::CHUNK_RADIUS.z; dz++)
  //   for (int dx = -ChunkManager::CHUNK_RADIUS.x; dx <= ChunkManager::CHUNK_RADIUS.x; dx++)
  //     for (int dy = -ChunkManager::CHUNK_RADIUS.y; dy <= ChunkManager::CHUNK_RADIUS.y; dy++)
  //       result.emplace_back(coord.x + dx, coord.y + dy, coord.z + dz);
  return result;
};

void GenerateVerticies(std::vector<std::vector<Vertex>>& results, Material material, SparseOctree<Voxel>* svo) {
  SparseOctree<Voxel>::Reader session = svo->BeginRead();

  std::vector<Vertex> buffer;

  static thread_local uint8_t padding[64 * 64] = {};
  std::memset(padding, 0, sizeof(padding));

  auto& rows    = svo->GetAxisX(session, material.Id);
  auto& columns = svo->GetAxisY(session, material.Id);
  auto& layers  = svo->GetAxisZ(session, material.Id);

  GreedyMesh64::GeneratePadding(padding, rows, columns, layers, [svo](int x, int y, int z) -> bool {
    if (x >= SparseOctree<Voxel>::SIZE || x < 0)
      return true;
    if (y >= SparseOctree<Voxel>::SIZE || y < 0)
      return true;
    if (z >= SparseOctree<Voxel>::SIZE || z < 0)
      return true;
    return svo->Exists(x, y, z);
  });

  GreedyMesh64::Generate(buffer, {0, 0, 0}, material.Id, rows, columns, layers, padding);

  results[material.Id - 1] = std::move(buffer);
};

void GreedyMesh(const std::vector<Material>& materials, SparseOctree<Voxel>* svo) {

  std::vector<std::vector<Vertex>> results(materials.size());

  for (auto& material : materials)
    GenerateVerticies(results, material, svo);
}

// void GenerateChunk(ChunkManager& chunkManager, const glm::ivec3& wcc, Voxel* voxel) {
//   uint64_t                m_ChunkSize = 64;
//   std::vector<glm::ivec3> lccs        = getLocalChunkCoordinates(wcc);
//   HeightMap               m_HeightMap;

//   m_HeightMap.Initialize(m_ChunkSize, m_ChunkSize);

//   for (auto& lcc : lccs) {
//     if (lcc.y == 0) {
//       auto session = chunkManager.BeginWrite(lcc);

//       auto noise = m_HeightMap.Build(lcc.x, lcc.x + 1.0f, lcc.z, lcc.z + 1.0f);

//       session.Root->Destroy();

//       for (int z = 0; z < m_ChunkSize; z++)
//         for (int x = 0; x < m_ChunkSize; x++) {
//           float n      = noise.GetValue(x, z);
//           int   height = static_cast<int>(std::round((std::clamp(n, -1.0f, 1.0f) + 1) * (m_ChunkSize / 2)));
//           // for (int y = 0; y < height; y++)
//           //   chunkManager.Set(lcc, session, x, y, z, voxel);
//           for (int y = 0; y < m_ChunkSize; y++) {
//             chunkManager.Set(lcc, session, x, y, z, voxel);
//             chunkManager.Sync(lcc);
//           }
//         }
//     }
//     // chunkManager.Sync(lcc);
//   }

//   chunkManager.Set({0, 0, 0}, m_ChunkSize / 2, m_ChunkSize - 4, m_ChunkSize / 2, voxel);
//   // chunkManager.Sync({0, 0, 0});
// }

int main(int argc, char** argv) {

  SparseOctree<Voxel> svo;
  Palette             m_Palette;
  uint64_t            m_ChunkSize = 64;

  m_Palette.Create(Palette::Item{
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material{.Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}}),
  });

  auto brickMaterial = m_Palette.Find("Brick");

  auto brick = std::make_shared<Voxel>(brickMaterial->Id);

  // GreedyMesh(m_Palette.GetMaterials(), &svo);

  // {
  //   SparseOctree<Voxel>::Writer session = svo.BeginWrite();

  //   for (size_t z = 0; z < 64; z++)
  //     for (size_t y = 0; y < 64; y++)
  //       for (size_t x = 0; x < 64; x++)
  //         svo.Set(session, x, y, z, brick.get());
  // }

  // svo.Sync();

  // {
  //   SparseOctree<Voxel>::Reader session = svo.BeginRead();

  //   for (size_t z = 0; z < 64; z++)
  //     for (size_t y = 0; y < 64; y++)
  //       for (size_t x = 0; x < 64; x++)
  //         svo.Get(session, x, y, z);
  // }

  // ChunkManager chunkManager(64);

  // GenerateChunk(chunkManager, {0, 0, 0}, brick.get());

  // ChunkManager<64, 4> chunkManager;
  
  // std::cout << (int)chunkManager.Wrap(74) << std::endl;

  auto sphericalMap = [](uint32_t x, uint32_t y, uint32_t z) {
    float theta = std::atan2(x, y);

    std::cout << theta << std::endl;
  };

  sphericalMap(1,9,1);

  return EXIT_SUCCESS;
}