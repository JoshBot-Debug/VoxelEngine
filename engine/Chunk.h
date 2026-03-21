#pragma once

#include <memory>
#include <vector>

#include "voxel/GreedyMesh64.h"
#include "voxel/SparseOctree.h"
#include "voxel/Voxel.h"

#include "thread/ThreadPool.h"

#include "render/Buffer.h"

namespace vxen {

template <uint32_t SS>
class Chunk {
  static_assert(SS != 0 && (SS & (SS - 1)) == 0, "SS must be a power of two");

public:
  static constexpr uint32_t SIZE = SS;

private:
  SparseOctree<Voxel, SS>* m_SVO {nullptr};
  uint8_t                  m_Padding[SIZE * SIZE] {0};

  std::vector<typename SparseOctree<Voxel, SS>::FlatNode> m_FlatNodes {};
  std::vector<Vertex>                                     m_Vertices {};
  std::vector<std::vector<Vertex>>                        m_ThreadVertices {};

public:
  uint32_t Id {0};

public:
  Chunk(uint32_t id)
      : m_SVO(new SparseOctree<Voxel, SS>())
      , Id(id) {};

  ~Chunk() {
    delete m_SVO;
  };

  bool Flush(std::shared_ptr<akari::thread::ThreadPool::Group> group, const glm::ivec3& offset, const std::vector<uint32_t>& ids);

  const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& FlatNodes();

  const std::vector<Vertex>& Verticies();

  SparseOctree<Voxel, SS>* SVO();
};

template <uint32_t SS>
inline bool Chunk<SS>::Flush(std::shared_ptr<akari::thread::ThreadPool::Group> group, const glm::ivec3& offset, const std::vector<uint32_t>& ids) {

  if (m_SVO->Empty() || !m_SVO->Dirty())
    return false;

  group->Add();

  m_ThreadVertices.clear();
  m_ThreadVertices.resize(ids.size());

  auto session = std::make_shared<typename SparseOctree<Voxel, SS>::Reader>(m_SVO->BeginRead(false));

  auto flatten = [session, &svo = m_SVO, &flatNodes = m_FlatNodes]() {
    svo->Flatten(*session, flatNodes);
  };

  auto greedyMesh = [session, offset, &padding = m_Padding, &svo = m_SVO, &tVertices = m_ThreadVertices](size_t i, uint32_t id) {
    auto& rows    = svo->GetAxisX(*session, id);
    auto& columns = svo->GetAxisY(*session, id);
    auto& layers  = svo->GetAxisZ(*session, id);

    GreedyMesh64::Generate(tVertices[i], offset, id, rows, columns, layers, padding);
  };

  auto onComplete = [group, session, &svo = m_SVO, &tVertices = m_ThreadVertices, &vertices = m_Vertices]() {
    vertices.clear();

    for (auto& v : tVertices)
      vertices.insert(vertices.end(), v.begin(), v.end());

    svo->Clean();
    group->Done();
    session->Unlock();
  };

  auto handle = akari::thread::ThreadPool::CreateGroup(onComplete);
  akari::thread::ThreadPool::ForEach(handle, ids, greedyMesh);
  akari::thread::ThreadPool::Dispatch(handle, flatten);

  return true;
}

template <uint32_t SS>
inline const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& Chunk<SS>::FlatNodes() {
  return m_FlatNodes;
}

template <uint32_t SS>
inline const std::vector<Vertex>& Chunk<SS>::Verticies() {
  return m_Vertices;
}

template <uint32_t SS>
inline SparseOctree<Voxel, SS>* Chunk<SS>::SVO() {
  return m_SVO;
}

} // namespace vxen
