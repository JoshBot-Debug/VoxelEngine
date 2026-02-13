#pragma once

#include <array>

#include "thread/ThreadPool.h"
#include "voxel/SparseOctree.h"
#include "voxel/Type.h"
#include "voxel/Voxel.h"

inline uint32_t radius(uint32_t chunkSize) {
  return (chunkSize - 1) / 2;
}

inline uint32_t index(const glm::ivec3& lcc, uint32_t chunkSize) {
  auto p = lcc + glm::ivec3(radius(chunkSize));
  return p.x + (chunkSize * (p.y + (chunkSize * p.z)));
}

inline uint32_t index(uint8_t x, uint8_t y, uint8_t z, uint32_t chunkSize) {
  uint32_t r = radius(chunkSize);
  x += r;
  y += r;
  z += r;
  return x + (chunkSize * (y + (chunkSize * z)));
}

inline int floorDivision(int a, int b) {
  return (a >= 0) ? (a / b) : ((a - b + 1) / b);
}

inline int wrap(int i, int size) {
  if (i == -1)
    return size - 1;
  if (i == size)
    return 0;
  return i;
}

class ChunkManager {
public:
  static constexpr uint32_t   SIZE         = 5;
  static constexpr glm::ivec3 CHUNK_RADIUS = glm::ivec3((SIZE - 1) >> 1);

private:
  std::array<SparseOctree<Voxel>*, SIZE * SIZE * SIZE> m_Chunks = {};
  SparseOctree<Voxel>*                        m_SVO    = nullptr;

  std::array<std::vector<std::vector<Vertex>>, SIZE * SIZE * SIZE> m_VertexThreads = {};

  std::array<akari::thread::ThreadPool::TaskId, SIZE * SIZE * SIZE> m_GreedyMeshingTask = {};

public:
  ChunkManager(uint32_t chunkSize);
  ~ChunkManager();

  void Set(const glm::ivec3& lcc, int x, int y, int z, Voxel* data);

  void Set(const glm::ivec3& lcc, const glm::ivec3& position, Voxel* data);

  void Set(const glm::ivec3& lcc, SparseOctree<Voxel>::Writer& session, int x, int y, int z, Voxel* data);

  void Clear(const glm::ivec3& lcc, const glm::ivec3& positions);

  void Sync(const glm::ivec3& lcc);

  SparseOctree<Voxel>::Reader BeginRead(const glm::ivec3& lcc);

  SparseOctree<Voxel>::Writer BeginWrite(const glm::ivec3& lcc);

  void Flatten(SparseOctree<Voxel>::Reader& session, const glm::ivec3& lcc, std::vector<SparseOctree<Voxel>::FlatNode>& out);

  void GreedyMesh(const glm::ivec3& lcc, const glm::ivec3& localChunkCoordinate, const std::vector<Material>& materials, std::vector<Vertex>& out);

  template <typename F>
    requires FilterCallback<SparseOctree<Voxel>::Node, F>
  void Filter(SparseOctree<Voxel>::Reader& session, const glm::ivec3& lcc, std::vector<SparseOctree<Voxel>::FilterNode>& out, F&& filter) {
    uint32_t i = index(lcc, SIZE);
    m_Chunks[i]->Filter(session, out, filter);
  };

  SparseOctree<Voxel>::Hit DeepRaymarch(const glm::ivec3& lcc, const glm::vec3& origin, const glm::vec3& direction);

  inline glm::ivec3 GetWorldChunkCoordinate(const glm::vec3& rayOrigin) {
    return {static_cast<int>(
                std::floor(rayOrigin.x / static_cast<float>(SparseOctree<Voxel>::SIZE))),
            static_cast<int>(
                std::floor(rayOrigin.y / static_cast<float>(SparseOctree<Voxel>::SIZE))),
            static_cast<int>(
                std::floor(rayOrigin.z / static_cast<float>(SparseOctree<Voxel>::SIZE)))};
  };
};