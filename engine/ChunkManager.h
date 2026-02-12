#pragma once

#include <array>

#include "thread/ThreadPool.h"
#include "voxel/SparseOctree.h"
#include "voxel/Type.h"
#include "voxel/Voxel.h"

class ChunkManager {
public:
  static constexpr uint32_t   SIZE          = 3;
  static constexpr glm::ivec3 CHUNK_RADIUS  = glm::ivec3(1);

private:
  std::array<SparseOctree<Voxel>*, 3 * 3 * 3> m_Chunks = {};
  SparseOctree<Voxel>*                        m_SVO    = nullptr;

  std::array<std::vector<std::vector<Vertex>>, 3 * 3 * 3> m_VertexThreads = {};

  std::array<akari::thread::ThreadPool::TaskId, 3 * 3 * 3> m_GreedyMeshingTask = {};

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
    m_SVO->Filter(session, out, filter);
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