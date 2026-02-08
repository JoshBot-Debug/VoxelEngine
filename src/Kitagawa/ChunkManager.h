#pragma once

#include <array>

#include "ThreadPool.h"
#include "Voxel/SparseOctree.h"
#include "Voxel/Type.h"
#include "Voxel/Voxel.h"

class ChunkManager {
private:
  static const uint32_t                              SIZE     = 2;
  std::array<SparseOctree<Voxel>*, SIZE* SIZE* SIZE> m_Chunks = {};
  SparseOctree<Voxel>*                               m_SVO    = nullptr;

  std::vector<std::vector<Vertex>> m_VertexThreads = {};

  Akari::ThreadPool::TaskId m_GreedyMeshingTask = Akari::ThreadPool::GenerateId();

private:
  inline glm::ivec3 GetCoord(int x, int y, int z);

public:
  ChunkManager(uint32_t chunkSize);
  ~ChunkManager();

  void Set(int x, int y, int z, Voxel* data);

  void Set(const glm::vec3& position, Voxel* data);

  void Clear(const glm::ivec3& positions);

  void Update(const glm::vec3& origin, const glm::vec3& direction);

  void Flush();

  bool IsDirty();

  void Clean();

  void Sync();

  uint64_t ReadLock();

  void ReadUnlock(uint64_t generation);

  void Flatten(std::vector<SparseOctree<Voxel>::FlatNode>& out);

  void GreedyMesh(const std::vector<Material>& materials, std::vector<Vertex>& out);

  template <typename F>
    requires FilterCallback<SparseOctree<Voxel>::Node, F>
  void Filter(std::vector<SparseOctree<Voxel>::FilterNode>& out, F&& filter) {
    m_SVO->Filter(out, filter);
  };

  SparseOctree<Voxel>::Hit DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction);
};