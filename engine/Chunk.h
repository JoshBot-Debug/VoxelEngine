#pragma once

#include <vector>

#include "voxel/SparseOctree.h"
#include "voxel/Type.h"

namespace vxen {

class ChunkManager;

class Chunk {
public:
  static constexpr uint32_t SIZE = 64;

private:
  SparseOctree* m_SVO {nullptr};
  uint8_t       m_Padding[SIZE * SIZE] {0};
  glm::u8vec3   m_Coordinate {};
  ChunkManager* m_ChunkManager {nullptr};

  std::vector<typename SparseOctree::FlatNode> m_FlatNodes {};
  std::vector<Vertex>                          m_Vertices {};
  std::vector<std::vector<Vertex>>             m_ThreadVertices {};

public:
  uint32_t Id {0};

public:
  Chunk(uint32_t id, const glm::u8vec3& coordinate, ChunkManager* chunkManager)
      : m_SVO(new SparseOctree())
      , Id(id)
      , m_Coordinate(coordinate)
      , m_ChunkManager(chunkManager) {}

  ~Chunk() {
    delete m_SVO;
  };

  Chunk(const Chunk&)            = delete;
  Chunk& operator=(const Chunk&) = delete;

  bool Flush(const std::vector<uint32_t>& ids);

  const std::vector<typename SparseOctree::FlatNode>& FlatNodes();

  const std::vector<Vertex>& Verticies();
  const glm::u8vec3&         GetCoordinate() { return m_Coordinate; };

  SparseOctree* SVO();
};

} // namespace vxen