#include "ChunkManager.h"

#include "voxel/Type.h"
#include "voxel/Voxel.h"

namespace vxen {

ChunkManager::ChunkManager() {
  m_SVOBuffer.SetPool(&m_SVOPool);
  m_SVOHeaderBuffer.SetPool(&m_SVOHeaderPool);
  m_VertexBuffer.SetPool(&m_VertexPool);

  m_SVOBuffer.CreateBuffer({.Size = 1024 * 1024 * 1024, .DebugName = "m_SVOBuffer"});
  m_SVOHeaderBuffer.CreateBuffer({.Size = 1024 * 1024 * 1024, .DebugName = "m_SVOHeaderBuffer"});
  m_VertexBuffer.CreateBuffer({
      .Size      = 1024 * 1024 * 1024,
      .Usage     = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .DebugName = "m_VertexBuffer",
  });

  m_ChunkBuffer.CreateBuffer({.DebugName = "m_ChunkBuffer"});
  m_ChunkSVOBuffer.CreateBuffer({.DebugName = "m_ChunkSVOBuffer"});
}

ChunkManager::Buffers ChunkManager::GetBuffers() {
  return {
      .SVO              = m_SVOBuffer.GetBuffer(),
      .SVOHeader        = m_SVOHeaderBuffer.GetBuffer(),
      .Vertex           = m_VertexBuffer.GetBuffer(),
      .ChunkSVO         = m_ChunkSVOBuffer.GetBuffer(),
      .Chunk            = m_ChunkBuffer.GetBuffer(),
      .SVOBarrier       = m_SVOBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT),
      .SVOHeaderBarrier = m_SVOHeaderBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT),
      .VertexBarrier    = m_VertexBuffer.GetBarrier(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT),
      .ChunkSVOBarrier  = m_ChunkSVOBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT),
      .ChunkBarrier     = m_ChunkBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT),
  };
}

void ChunkManager::Set(uint32_t x, uint32_t y, uint32_t z, uint32_t id) {
  m_SCO.Set(x, y, z, id);
}

Node* ChunkManager::Get(uint32_t x, uint32_t y, uint32_t z) {
  return m_SCO.Get(x, y, z);
}

bool ChunkManager::Exists(uint32_t x, uint32_t y, uint32_t z) {
  return m_SCO.Exists(x, y, z);
}

void ChunkManager::Clear(uint32_t x, uint32_t y, uint32_t z) {
  m_SCO.Clear(x, y, z);
}

SparseOctree::Hit ChunkManager::Raymarch(const glm::vec3& origin, const glm::vec3& direction) {
  return m_SCO.Raymarch(origin, direction);
}

void ChunkManager::FlushVertices(const std::vector<uint32_t>& ids) {
  // auto start    = std::chrono::steady_clock::now();
  // m_Vertices    = std::move(m_SCO.GreedyMesh());
  // auto end      = std::chrono::steady_clock::now();
  // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  // std::cout << "GreedyMesh(): " << duration.count() << "ms" << std::endl;
}

void ChunkManager::FlushPreprocessor(VkCommandBuffer commandBuffer) {
  using namespace akari::render;

  constexpr uint32_t SIZE = SparseOctree::SIZE;

  if (m_SCO.SVO()->Dirty())
    m_SCO.SVO()->Flatten(m_SCONodes);

  m_ChunkState.clear();
  m_ChunkState.resize(SIZE * SIZE * SIZE);

  std::vector<Vertex>            vertices;
  std::vector<Buffer::Partition> vertexChunkPartitions;

  std::vector<SparseOctree::FlatNode> svo;
  std::vector<Buffer::Partition>      svoChunkPartitions;
  std::vector<FlatNodeHeader>         svoChunkPartitionHeaders;

  auto& chunks = m_SCO.Clusters();

  for (size_t i = 0; i < chunks.size(); i++) {
    auto& chunk      = chunks[i];
    bool  dirty      = chunk->Dirty();
    auto  coordinate = chunk->GetCoordinate();

    auto& v        = m_SCO.GreedyMesh(coordinate.x, coordinate.y, coordinate.z);
    vertices.insert(vertices.end(), v.begin(), v.end());
    vertexChunkPartitions.emplace_back(i + 1, v.size() * sizeof(Vertex), dirty);

    std::vector<SparseOctree::FlatNode> s;
    chunk->Flatten(s);
    svo.insert(svo.end(), s.begin(), s.end());
    svoChunkPartitions.emplace_back(i + 1, s.size() * sizeof(SparseOctree::FlatNode), dirty);
    chunk->Clean();
  }

  auto svoAllocations    = m_SVOBuffer.Upload(commandBuffer, svoChunkPartitions, svo.data());
  auto vertexAllocations = m_VertexBuffer.Upload(commandBuffer, vertexChunkPartitions, vertices.data());

  for (size_t i = 0; i < chunks.size(); i++) {
    // Vertex allocation partition data
    Buffer::Allocation& va            = vertexAllocations[i];
    m_ChunkState[va.Id - 1].Empty     = 0;
    m_ChunkState[va.Id - 1].Offset[0] = va.Offset / sizeof(Vertex);
    m_ChunkState[va.Id - 1].Size[0]   = va.Size / sizeof(Vertex);

    // SVO allocation partition data
    Buffer::Allocation& sa     = svoAllocations[i];
    uint32_t            offset = sa.Offset / sizeof(SparseOctree::FlatNode);
    uint32_t            size   = sa.Size / sizeof(SparseOctree::FlatNode);
    svoChunkPartitionHeaders.emplace_back(sa.Id, offset, size);
  }

  m_SVOHeaderBuffer.Upload(commandBuffer, svoChunkPartitionHeaders);
  m_ChunkSVOBuffer.Upload(commandBuffer, m_SCONodes);
  m_ChunkBuffer.Upload(commandBuffer, m_ChunkState);
}
} // namespace vxen
