#pragma once

#include <array>

#include "ThreadPool.h"
#include "Voxel/SparseOctree.h"
#include "Voxel/Type.h"
#include "Voxel/Voxel.h"

class ChunkManager {
public:
  static constexpr uint32_t   SIZE          = 3;
  static constexpr glm::ivec3 CHUNK_RADIUS  = glm::ivec3(1);
  static constexpr glm::ivec3 ROOT_POSITION = glm::ivec3(1);

private:
  std::array<SparseOctree<Voxel>*, 3 * 3 * 3> m_Chunks = {};
  SparseOctree<Voxel>*                        m_SVO    = nullptr;

  std::vector<std::vector<Vertex>> m_VertexThreads = {};

  Akari::ThreadPool::TaskId m_GreedyMeshingTask = Akari::ThreadPool::GenerateId();

public:
  ChunkManager(uint32_t chunkSize);
  ~ChunkManager();

  void Set(const glm::ivec3& coord, int x, int y, int z, Voxel* data);

  void Set(const glm::ivec3& coord, const glm::vec3& position, Voxel* data);

  void Set(const glm::ivec3& coord, SparseOctree<Voxel>::Writer& session, int x, int y, int z, Voxel* data);

  void Clear(const glm::ivec3& coord, const glm::ivec3& positions);

  void Sync(const glm::ivec3& coord);

  SparseOctree<Voxel>::Reader BeginRead(const glm::ivec3& coord);

  SparseOctree<Voxel>::Writer BeginWrite(const glm::ivec3& coord);

  void Flatten(SparseOctree<Voxel>::Reader& session, const glm::ivec3& coord, std::vector<SparseOctree<Voxel>::FlatNode>& out);

  void GreedyMesh(const glm::ivec3& coord, const std::vector<Material>& materials, std::vector<Vertex>& out);

  template <typename F>
    requires FilterCallback<SparseOctree<Voxel>::Node, F>
  void Filter(SparseOctree<Voxel>::Reader& session, const glm::ivec3& coord, std::vector<SparseOctree<Voxel>::FilterNode>& out, F&& filter) {
    m_SVO->Filter(session, out, filter);
  };

  SparseOctree<Voxel>::Hit DeepRaymarch(const glm::ivec3& coord, const glm::vec3& origin, const glm::vec3& direction);

  inline glm::ivec3 GetChunkCoord(const glm::vec3& rayOrigin) {
    return {static_cast<int>(
                std::floor(rayOrigin.x / static_cast<float>(SparseOctree<Voxel>::SIZE))),
            static_cast<int>(
                std::floor(rayOrigin.y / static_cast<float>(SparseOctree<Voxel>::SIZE))),
            static_cast<int>(
                std::floor(rayOrigin.z / static_cast<float>(SparseOctree<Voxel>::SIZE)))};
  };
};