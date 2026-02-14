#pragma once

#include <vector>

#include "voxel/GreedyMesh64.h"
#include "voxel/SparseOctree.h"
#include "voxel/Voxel.h"

template <typename F>
concept Exists = requires(F& f, int x, int y, int z) {
  { f(x, y, z) } -> std::same_as<bool>;
};

template <uint32_t SS>
class Chunk {
public:
  static constexpr uint32_t SIZE = SS;

private:
  SparseOctree<Voxel, SS>*                                m_SVO            = nullptr;
  std::vector<typename SparseOctree<Voxel, SS>::FlatNode> m_FlatNodes      = {};
  std::vector<Vertex>                                     m_Vertices       = {};
  std::vector<std::vector<Vertex>>                        m_ThreadVertices = {};

  static uint32_t UID() {
    static uint32_t uid = 1;
    return uid++;
  }

public:
  uint32_t Id = 0;

public:
  Chunk()
      : m_SVO(new SparseOctree<Voxel, SS>())
      , Id(UID()){};
  ~Chunk() { delete m_SVO; };

  const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& Flatten() {
    m_SVO->Flatten(m_FlatNodes);
    return m_FlatNodes;
  }

  const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& Flatten(typename SparseOctree<Voxel, SS>::Reader& session) {
    m_SVO->Flatten(session, m_FlatNodes);
    return m_FlatNodes;
  }

  template <Exists E>
  const std::vector<Vertex>& GreedyMesh(const glm::ivec3& offset, const std::vector<uint32_t>& ids, E& exists) {
    m_ThreadVertices.resize(ids.size());

    auto generateVerticies = [&](size_t i, uint32_t id) {
      auto session = m_SVO->BeginRead();

      uint8_t padding[SIZE * SIZE] = {};
      auto&   rows                 = m_SVO->GetAxisX(session, id);
      auto&   columns              = m_SVO->GetAxisY(session, id);
      auto&   layers               = m_SVO->GetAxisZ(session, id);

      GreedyMesh64::GeneratePadding(padding, rows, columns, layers, [&svo = m_SVO, size = SIZE, &exists](int x, int y, int z) -> bool {
        if (x < 0 || y < 0 || z < 0 || x >= size || y >= size || z >= size)
          return exists(x, y, z);
        return svo->Exists(x, y, z);
      });

      GreedyMesh64::Generate(m_ThreadVertices[i], offset, id, rows, columns, layers, padding);
    };

    auto onComplete = [&]() {
      size_t total = 0;

      for (const auto& v : m_ThreadVertices)
        total += v.size();

      m_Vertices.clear();
      m_Vertices.reserve(total);

      for (auto& v : m_ThreadVertices)
        m_Vertices.insert(m_Vertices.end(), v.begin(), v.end());

      for (auto& v : m_ThreadVertices)
        v.clear();
    };

    for (size_t i = 0; i < ids.size(); i++)
      generateVerticies(i, ids[i]);

    onComplete();

    return m_Vertices;
  }
};