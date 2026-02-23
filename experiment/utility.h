#pragma once

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

inline void GenerateVerticies(std::vector<std::vector<Vertex>>& results, Material material, SparseOctree<Voxel>* svo) {
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

inline void GreedyMesh(const std::vector<Material>& materials, SparseOctree<Voxel>* svo) {

  std::vector<std::vector<Vertex>> results(materials.size());

  for (auto& material : materials)
    GenerateVerticies(results, material, svo);
}
