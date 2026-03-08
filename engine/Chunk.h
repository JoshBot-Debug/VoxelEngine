#pragma once

#include <vector>

#include "voxel/GreedyMesh64.h"
#include "voxel/SparseOctree.h"
#include "voxel/Voxel.h"

#include "thread/Signal.h"
#include "thread/ThreadPool.h"

#include "render/Buffer.h"
#include "render/BufferPool.h"
#include "state/StateMachine.h"

#include "Signal.h"

namespace vxen {

template <uint32_t SS>
class Chunk {
  static_assert(SS != 0 && (SS & (SS - 1)) == 0, "SS must be a power of two");

public:
  static constexpr uint32_t SIZE = SS;

  struct FlushedChunk {
    bool                   VerticesResized {0};
    bool                   SVOResized {0};
    uint32_t               VertexOffset {0};
    uint32_t               SVOOffset {0};
    uint32_t               VertexCount {0};
    VkBufferMemoryBarrier2 VertexBarrier {};
    VkBufferMemoryBarrier2 SVOBarrier {};
    VkBuffer               SVOBuffer {VK_NULL_HANDLE};
    VkBuffer               VertexBuffer {VK_NULL_HANDLE};
  };

private:
  SparseOctree<Voxel, SS>* m_SVO {nullptr};
  uint8_t                  m_Padding[SIZE * SIZE] {0};

  std::vector<typename SparseOctree<Voxel, SS>::FlatNode> m_FlatNodes {};
  std::vector<Vertex>                                     m_Vertices {};
  std::vector<std::vector<Vertex>>                        m_ThreadVertices {};

  akari::render::Buffer m_SVOBuffer {};
  akari::render::Buffer m_VertexBuffer {{.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT}};

  FlushedChunk m_FlushedChunk;

  static uint32_t UID();

public:
  uint32_t Id {0};

public:
  Chunk(akari::render::BufferPool* svoPool, akari::render::BufferPool* vertexPool)
      : m_SVO(new SparseOctree<Voxel, SS>())
      , Id(UID()) {
    m_SVOBuffer.SetPool(svoPool);
    m_VertexBuffer.SetPool(vertexPool);

    m_SVOBuffer.CreateBuffer();
    m_VertexBuffer.CreateBuffer();
  };

  ~Chunk() {
    delete m_SVO;
  };

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

  const std::vector<Vertex>& GreedyMesh(const glm::ivec3& offset, const std::vector<uint32_t>& ids);

  template <typename F>
    requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
  void Filter(std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter);

  template <typename F>
    requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
  void Filter(SparseOctree<Voxel, SS>::Reader& session, std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter);

  SparseOctree<Voxel, SS>::Hit DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction);

  SparseOctree<Voxel, SS>::Hit DeepRaymarch(SparseOctree<Voxel, SS>::Reader& session, const glm::vec3& origin, const glm::vec3& direction);

  void FlushUpdates(const glm::ivec3& offset, const std::vector<uint32_t>& ids);

  const FlushedChunk& FlushRenderer(VkCommandBuffer commandBuffer);
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
inline const std::vector<Vertex>& Chunk<SS>::GreedyMesh(const glm::ivec3& offset, const std::vector<uint32_t>& ids) {
  m_ThreadVertices.resize(ids.size());

  auto greedyMesh = [&](size_t i, uint32_t id) {
    auto session = m_SVO->BeginRead();

    auto& rows    = m_SVO->GetAxisX(session, id);
    auto& columns = m_SVO->GetAxisY(session, id);
    auto& layers  = m_SVO->GetAxisZ(session, id);

    GreedyMesh64::Generate(m_ThreadVertices[i], offset, id, rows, columns, layers, m_Padding);
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
    greedyMesh(i, ids[i]);

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
}

template <uint32_t SS>
inline void Chunk<SS>::FlushUpdates(const glm::ivec3& offset, const std::vector<uint32_t>& ids) {
  m_ThreadVertices.resize(ids.size());

  auto flatten = [&]() {
    m_SVO->Flatten(m_FlatNodes);
  };

  auto greedyMesh = [svo = m_SVO, &padding = m_Padding, offset, &vertices = m_ThreadVertices](size_t i, uint32_t id) {
    auto session = svo->BeginRead();

    auto& rows    = svo->GetAxisX(session, id);
    auto& columns = svo->GetAxisY(session, id);
    auto& layers  = svo->GetAxisZ(session, id);

    GreedyMesh64::Generate(vertices[i], offset, id, rows, columns, layers, padding);
  };

  auto group = akari::thread::ThreadPool::CreateGroup([&]() {
    size_t total = 0;

    for (const auto& v : m_ThreadVertices)
      total += v.size();

    m_Vertices.clear();
    m_Vertices.reserve(total);

    for (auto& v : m_ThreadVertices)
      m_Vertices.insert(m_Vertices.end(), v.begin(), v.end());

    for (auto& v : m_ThreadVertices)
      v.clear();

    TSignal::Set(0, CHUNK_MANAGER_FLUSH_RENDER);
  });

  akari::thread::ThreadPool::ForEach(group, ids, greedyMesh);

  akari::thread::ThreadPool::Dispatch(group, flatten);
}

template <uint32_t SS>
inline const Chunk<SS>::FlushedChunk& Chunk<SS>::FlushRenderer(VkCommandBuffer commandBuffer) {
  /// TODO: Prepare the VkBuffers
  auto vertexAlloc = m_VertexBuffer.Upload(commandBuffer, m_Vertices.size() * sizeof(Vertex), m_Vertices.data());
  auto svoAlloc    = m_SVOBuffer.Upload(commandBuffer, m_FlatNodes);

  m_FlushedChunk.VertexOffset = vertexAlloc.Offset;
  m_FlushedChunk.SVOOffset    = svoAlloc.Offset;

  m_FlushedChunk.VerticesResized = vertexAlloc.Resized;
  m_FlushedChunk.SVOResized      = svoAlloc.Resized;
  m_FlushedChunk.VertexCount     = static_cast<uint64_t>(m_Vertices.size());

  m_FlushedChunk.VertexBarrier = m_VertexBuffer.GetBarrier(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT);
  m_FlushedChunk.SVOBarrier    = m_SVOBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

  m_FlushedChunk.VertexBuffer = m_VertexBuffer.GetBuffer();
  m_FlushedChunk.SVOBuffer    = m_SVOBuffer.GetBuffer();

  return m_FlushedChunk;
}

} // namespace vxen
