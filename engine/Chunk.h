#pragma once

#include <memory>
#include <vector>

#include <algorithm>
#include <execution>
#include <future>

#include "voxel/GreedyMesh64.h"
#include "voxel/SparseOctree.h"
#include "voxel/Voxel.h"

#include "thread/ThreadPool.h"

#include "render/Buffer.h"

namespace vxen {

class Chunk {
public:
  static constexpr uint32_t SIZE = 64;

private:
  SparseOctree* m_SVO {nullptr};
  uint8_t       m_Padding[SIZE * SIZE] {0};

  std::vector<typename SparseOctree::FlatNode> m_FlatNodes {};
  std::vector<Vertex>                          m_Vertices {};
  std::vector<std::vector<Vertex>>             m_ThreadVertices {};

public:
  uint32_t Id {0};

public:
  Chunk(uint32_t id)
      : m_SVO(new SparseOctree())
      , Id(id) {};

  ~Chunk() {
    delete m_SVO;
  };

  Chunk(const Chunk&)            = delete;
  Chunk& operator=(const Chunk&) = delete;

  bool Flush(const glm::ivec3& offset, const std::vector<uint32_t>& ids);

  const std::vector<typename SparseOctree::FlatNode>& FlatNodes();

  const std::vector<Vertex>& Verticies();

  SparseOctree* SVO();
};

inline bool Chunk::Flush(const glm::ivec3& offset, const std::vector<uint32_t>& ids) {

  if (m_SVO->Empty() || !m_SVO->Dirty())
    return false;

  m_ThreadVertices.clear();
  m_ThreadVertices.resize(ids.size());

  auto session = std::make_shared<SparseOctree::Reader>(m_SVO->BeginRead(false));

  auto flatten = [session, &svo = m_SVO, &flatNodes = m_FlatNodes]() {
    svo->Flatten(*session, flatNodes);
  };

  auto greedyMesh = [session, offset, &padding = m_Padding, &svo = m_SVO, &tVertices = m_ThreadVertices](size_t i, uint32_t id) {
    auto& rows    = svo->GetAxisX(*session, id);
    auto& columns = svo->GetAxisY(*session, id);
    auto& layers  = svo->GetAxisZ(*session, id);

    GreedyMesh64::Generate(tVertices[i], offset, id, rows, columns, layers, padding);
  };

  auto onComplete = [session, &svo = m_SVO, &tVertices = m_ThreadVertices, &vertices = m_Vertices]() {
    vertices.clear();

    for (auto& v : tVertices)
      vertices.insert(vertices.end(), v.begin(), v.end());

    svo->Clean();
    session->Unlock();
  };

  for (size_t i = 0; i < ids.size(); i++)
    greedyMesh(i, ids[i]);
  flatten();
  onComplete();

  return true;
}

inline const std::vector<typename SparseOctree::FlatNode>& Chunk::FlatNodes() {
  return m_FlatNodes;
}

inline const std::vector<Vertex>& Chunk::Verticies() {
  return m_Vertices;
}

inline SparseOctree* Chunk::SVO() {
  return m_SVO;
}

} // namespace vxen
