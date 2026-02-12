#include <iostream>
#include <vector>

#include "Debug.h"
#include "voxel/GreedyMesh64.h"
#include "voxel/Palette.h"
#include "voxel/SparseOctree.h"
#include "voxel/Voxel.h"

void GenerateVerticies(std::vector<std::vector<Vertex>>& results, Material material, SparseOctree<Voxel>::Node* root, SparseOctree<Voxel>* svo) {
  SparseOctree<Voxel>::Reader session = svo->BeginRead();

  std::vector<Vertex> buffer;

  static thread_local uint8_t padding[64 * 64] = {};
  std::memset(padding, 0, sizeof(padding));

  auto& rows    = svo->GetAxisX(session.Root, material.Id);
  auto& columns = svo->GetAxisY(session.Root, material.Id);
  auto& layers  = svo->GetAxisZ(session.Root, material.Id);

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

  SparseOctree<Voxel>::Node* root = svo->GetRoot(std::memory_order::acquire);

  for (auto& material : materials)
    GenerateVerticies(results, material, root, svo);
}

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

  {
    SparseOctree<Voxel>::Writer session = svo.BeginWrite();

    for (size_t z = 0; z < 64; z++)
      for (size_t y = 0; y < 64; y++)
        for (size_t x = 0; x < 64; x++)
          svo.Set(session, x, y, z, brick.get());
  }

  svo.Sync();

  {
    SparseOctree<Voxel>::Reader session = svo.BeginRead();

    for (size_t z = 0; z < 64; z++)
      for (size_t y = 0; y < 64; y++)
        for (size_t x = 0; x < 64; x++)
          svo.Get(session, x, y, z);
  }

  return EXIT_SUCCESS;
}