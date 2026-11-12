#include "Chunk.h"

#include "voxel/GreedyMesh64.h"
#include "voxel/Voxel.h"

#include "render/Buffer.h"
#include "thread/ThreadPool.h"

#include "ChunkManager.h"

namespace vxen {

bool Chunk::Flush(const std::vector<uint32_t>& ids) {

  if (m_SVO->Empty() || !m_SVO->Dirty())
    return false;

  m_ThreadVertices.clear();
  m_ThreadVertices.resize(ids.size());

  auto session = std::make_shared<SparseOctree::Reader>(m_SVO->BeginRead(false));

  auto greedyMesh = [&](size_t i, uint32_t id) {
    auto& rows    = m_SVO->GetAxisX(*session, id);
    auto& columns = m_SVO->GetAxisY(*session, id);
    auto& layers  = m_SVO->GetAxisZ(*session, id);

    // GreedyMesh64::GeneratePadding(m_Padding, rows, columns, layers, [&](int x, int y, int z) {
    //   glm::i32vec3 position = glm::i32vec3 {(m_Coordinate.x * SIZE) + x, (m_Coordinate.y * SIZE) + y, (m_Coordinate.z * SIZE) + z};
    //   if (position.x == -1 || position.y == -1 || position.z == -1)
    //     return false;
    //   return m_ChunkManager->Exists(position.x, position.y, position.z);
    // });

    GreedyMesh64::Generate(m_ThreadVertices[i], m_Coordinate, id, rows, columns, layers, m_Padding);
  };

  auto onComplete = [&]() {
    m_SVO->Flatten(*session, m_FlatNodes);

    m_Vertices.clear();

    for (auto& v : m_ThreadVertices)
      m_Vertices.insert(m_Vertices.end(), v.begin(), v.end());

    session->Unlock();
  };

  for (size_t i = 0; i < ids.size(); i++)
    greedyMesh(i, ids[i]);
  // greedyMesh(0, 0);
  onComplete();

  return true;
}

const std::vector<typename SparseOctree::FlatNode>& Chunk::FlatNodes() {
  return m_FlatNodes;
}

const std::vector<Vertex>& Chunk::Verticies() {
  return m_Vertices;
}

SparseOctree* Chunk::SVO() {
  return m_SVO;
}
} // namespace vxen