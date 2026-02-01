#pragma once

#include <array>

#include "Voxel/SparseOctree.h"
#include "Voxel/Type.h"
#include "Voxel/Voxel.h"

class ChunkManager {
private:
  static const uint32_t                              SIZE     = 16;
  std::array<SparseOctree<Voxel>*, SIZE* SIZE* SIZE> m_Chunks = {};

public:
  ChunkManager(uint32_t chunkSize);
  ~ChunkManager();

  void Set(int x, int y, int z, Voxel* data, int leafSize = 1);

  void Set(const glm::vec3& position, Voxel* data, int leafSize = 1);

  void Clear(const glm::ivec3& position, int leafSize);

  void Update(const glm::vec3& origin, const glm::vec3& direction);

  void Flush();

  bool IsDirty();

  void Clean();

  void Flatten(std::vector<SparseOctree<Voxel>::FlatNode>& out);

  void GreedyMesh(const std::vector<Material>& materials, std::vector<Vertex> out);

  template <typename F>
    requires FilterCallback<SparseOctree<Voxel>::Node, F>
  void Filter(std::vector<SparseOctree<Voxel>::FilterNode>& out, F&& filter){

  };

  SparseOctree<Voxel>::Hit DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction);
};