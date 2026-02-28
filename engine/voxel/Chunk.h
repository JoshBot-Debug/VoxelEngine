#pragma once

#include <vector>

#include "voxel/GreedyMesh64.h"
#include "voxel/SparseOctree.h"
#include "voxel/Voxel.h"

template <typename F>
concept NeighbourExists = requires(F& f, int x, int y, int z) {
  { f(x, y, z) } -> std::same_as<bool>;
};

template <uint32_t SS>
class Chunk {
  static_assert(SS != 0 && (SS & (SS - 1)) == 0, "SS must be a power of two");

public:
  static constexpr uint32_t SIZE = SS;

private:
  SparseOctree<Voxel, SS>*                                m_SVO {nullptr};
  std::vector<typename SparseOctree<Voxel, SS>::FlatNode> m_FlatNodes {};
  std::vector<Vertex>                                     m_Vertices {};
  std::vector<std::vector<Vertex>>                        m_ThreadVertices {};

  static uint32_t UID();

public:
  uint32_t Id {0};

public:
  Chunk()
      : m_SVO(new SparseOctree<Voxel, SS>())
      , Id(UID()) {};
  ~Chunk() { delete m_SVO; };

  SparseOctree<Voxel, SS>::Reader BeginRead();

  SparseOctree<Voxel, SS>::Writer BeginWrite();

  void Sync();

  void Set(uint8_t x, uint8_t y, uint8_t z, Voxel* data);

  void Set(SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z, Voxel* data);

  SparseOctree<Voxel, SS>::Node Get(uint8_t x, uint8_t y, uint8_t z);

  SparseOctree<Voxel, SS>::Node Get(SparseOctree<Voxel, SS>::Reader& session, uint8_t x, uint8_t y, uint8_t z);

  void Clear(uint8_t x, uint8_t y, uint8_t z);

  void Clear(SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z);

  bool Exists(int x, int y, int z);

  bool Exists(uint8_t x, uint8_t y, uint8_t z);

  const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& Flatten();

  const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& Flatten(SparseOctree<Voxel, SS>::Reader& session);

  template <NeighbourExists E>
  const std::vector<Vertex>& GreedyMesh(const glm::ivec3& offset, const std::vector<uint32_t>& ids, E& exists);

  template <typename F>
    requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
  void Filter(std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter);

  template <typename F>
    requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
  void Filter(SparseOctree<Voxel, SS>::Reader& session, std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter);

  SparseOctree<Voxel, SS>::Hit DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction);

  SparseOctree<Voxel, SS>::Hit DeepRaymarch(SparseOctree<Voxel, SS>::Reader& session, const glm::vec3& origin, const glm::vec3& direction);
};

template <uint32_t SS>
inline uint32_t Chunk<SS>::UID() {
  static uint32_t uid = 1;
  return uid++;
}

template <uint32_t SS>
inline SparseOctree<Voxel, SS>::Reader Chunk<SS>::BeginRead() {
  return m_SVO->BeginRead();
}

template <uint32_t SS>
inline SparseOctree<Voxel, SS>::Writer Chunk<SS>::BeginWrite() {
  return m_SVO->BeginWrite();
}

template <uint32_t SS>
inline void Chunk<SS>::Sync() {
  m_SVO->Sync();
}

template <uint32_t SS>
inline void Chunk<SS>::Set(uint8_t x, uint8_t y, uint8_t z, Voxel* data) {
  m_SVO->Set(x, y, z, data);
}

template <uint32_t SS>
inline void Chunk<SS>::Set(SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z, Voxel* data) {
  m_SVO->Set(session, x, y, z, data);
}

template <uint32_t SS>
inline SparseOctree<Voxel, SS>::Node Chunk<SS>::Get(uint8_t x, uint8_t y, uint8_t z) {
  return m_SVO->Get(x, y, z);
}

template <uint32_t SS>
inline SparseOctree<Voxel, SS>::Node Chunk<SS>::Get(SparseOctree<Voxel, SS>::Reader& session, uint8_t x, uint8_t y, uint8_t z) {
  return m_SVO->Get(session, x, y, z);
}

template <uint32_t SS>
inline void Chunk<SS>::Clear(uint8_t x, uint8_t y, uint8_t z) {
  m_SVO->Clear(x, y, z);
}

template <uint32_t SS>
inline void Chunk<SS>::Clear(SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z) {
  m_SVO->Clear(session, x, y, z);
}

template <uint32_t SS>
inline bool Chunk<SS>::Exists(int x, int y, int z) {
  return m_SVO->Exists(x, y, z);
}

template <uint32_t SS>
inline bool Chunk<SS>::Exists(uint8_t x, uint8_t y, uint8_t z) {
  return m_SVO->Exists(x, y, z);
}

template <uint32_t SS>
inline const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& Chunk<SS>::Flatten() {
  m_SVO->Flatten(m_FlatNodes);
  return m_FlatNodes;
}

template <uint32_t SS>
inline const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& Chunk<SS>::Flatten(typename SparseOctree<Voxel, SS>::Reader& session) {
  m_SVO->Flatten(session, m_FlatNodes);
  return m_FlatNodes;
}

template <uint32_t SS>
template <NeighbourExists E>
inline const std::vector<Vertex>& Chunk<SS>::GreedyMesh(const glm::ivec3& offset, const std::vector<uint32_t>& ids, E& exists) {
  m_ThreadVertices.resize(ids.size());

  auto generateVerticies = [&](size_t i, uint32_t id) {
    auto session = m_SVO->BeginRead();

    uint8_t padding[SIZE * SIZE] = {};
    auto&   rows                 = m_SVO->GetAxisX(session, id);
    auto&   columns              = m_SVO->GetAxisY(session, id);
    auto&   layers               = m_SVO->GetAxisZ(session, id);

    GreedyMesh64::GeneratePadding(padding, rows, columns, layers, [&svo = m_SVO, size = SIZE, &exists](int x, int y, int z) -> bool {
      if (x < 0 || y < 0 || z < 0 || x == size || y == size || z == size)
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

template <uint32_t SS>
template <typename F>
  requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
inline void Chunk<SS>::Filter(std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter) {
  return m_SVO->Filter(out, filter);
}

template <uint32_t SS>
template <typename F>
  requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
inline void Chunk<SS>::Filter(SparseOctree<Voxel, SS>::Reader& session, std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter) {
  return m_SVO->Filter(session, out, filter);
}

template <uint32_t SS>
inline SparseOctree<Voxel, SS>::Hit Chunk<SS>::DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction) {
  return m_SVO->DeepRaymarch(origin, direction);
}

template <uint32_t SS>
inline SparseOctree<Voxel, SS>::Hit Chunk<SS>::DeepRaymarch(SparseOctree<Voxel, SS>::Reader& session, const glm::vec3& origin, const glm::vec3& direction) {
  return m_SVO->DeepRaymarch(session, origin, direction);
};
