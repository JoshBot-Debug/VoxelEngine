#include "ChunkManager.h"

#include "voxel/Type.h"
#include "voxel/Voxel.h"

namespace vxen {

Node* ChunkManager::Ensure(uint32_t x, uint32_t y, uint32_t z) {
  Node* chunk = m_SCO->Get(x, y, z);
  if (chunk)
    return chunk;
  uint32_t cid = m_ChunkAllocations.size() + 1;
  m_ChunkAllocations.emplace_back(cid, glm::u32vec3 {x, y, z}, this);
  m_SCO->Set(x, y, z, cid);
  return m_SCO->Get(x, y, z);
}

ChunkManager::ChunkManager()
    : m_SCO(new SparseOctree()) {
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

ChunkManager::~ChunkManager() {
  delete m_SCO;
}

const std::vector<SparseOctree::FlatNode>& ChunkManager::Flatten() {
  m_SCO->Flatten(m_SCONodes);
  return m_SCONodes;
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
  const glm::u32vec3 coordinate {x / SVO_SIZE, y / SVO_SIZE, z / SVO_SIZE};
  auto               chunkId = Ensure(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Set(x % SVO_SIZE, y % SVO_SIZE, z % SVO_SIZE, id);
}

Node* ChunkManager::Get(uint32_t x, uint32_t y, uint32_t z) {
  const glm::u32vec3 coordinate {x / SVO_SIZE, y / SVO_SIZE, z / SVO_SIZE};
  auto               chunkNode = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  if (!chunkNode)
    return nullptr;
  uint32_t chunkId = chunkNode->Id - 1;
  return m_ChunkAllocations[chunkId].SVO()->Get(x % SVO_SIZE, y % SVO_SIZE, z % SVO_SIZE);
}

bool ChunkManager::Exists(uint32_t x, uint32_t y, uint32_t z) {
  const glm::u32vec3 coordinate {x / SVO_SIZE, y / SVO_SIZE, z / SVO_SIZE};
  auto               chunkNode = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  if (!chunkNode)
    return false;
  uint32_t chunkId = chunkNode->Id - 1;
  return m_ChunkAllocations[chunkId].SVO()->Exists(x % SVO_SIZE, y % SVO_SIZE, z % SVO_SIZE);
}

void ChunkManager::Clear(uint32_t x, uint32_t y, uint32_t z) {
  const glm::u32vec3 coordinate {x / SVO_SIZE, y / SVO_SIZE, z / SVO_SIZE};
  auto               chunkNode = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  if (!chunkNode)
    return;
  uint32_t chunkId = chunkNode->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Clear(x % SVO_SIZE, y % SVO_SIZE, z % SVO_SIZE);
}

SparseOctree::Hit ChunkManager::Raymarch(const glm::vec3& origin, const glm::vec3& direction) {
  float           t    = 0.0f;
  float           EPS  = 0.001f;
  constexpr float SIZE = SVO_SIZE;

  auto step = [&](auto&& self, float& t) {
    glm::vec3 P = origin + direction * t;

    SparseOctree::Hit chunk = m_SCO->DeepRaymarch(P / SIZE, direction);

    if (!chunk.Id)
      return SparseOctree::Hit();

    // Chunk hit distance relative to current origin
    // 0.001f is to nudge the ray forward a bit so we don't get stuck outside the chunk
    // chunkT moves us into the first chunk we find, we should no get stuck at 64.0f, EPS moves us to 63.0f
    float chunkT = (chunk.TMin * SIZE) + EPS;

    // Ray position inside chunk
    glm::vec3 chunkOrigin = glm::mod(P + direction * chunkT, SIZE);

    SparseOctree::Hit hit = m_ChunkAllocations[chunk.Id - 1].SVO()->DeepRaymarch(chunkOrigin, direction);

    if (hit.Id) {
      hit.TMin += chunkT + t;
      hit.Position += chunk.Position * SIZE;
      return hit;
    }

    // Distance available inside this chunk
    float chunkDist = TNextBoundary(chunkOrigin, direction);

    // Continue past this chunk
    // EPS is to nudge the ray 1 block passed where ever it hits, so we don't get stuck on a boundary inside the same chunk.
    // It moves us to the next chunk (that's the purpose of TNextBoundary but it returns the exact value)
    t += chunkT + chunkDist + EPS;

    return self(self, t);
  };

  return step(step, t);
}

void ChunkManager::FlushVertices(const std::vector<uint32_t>& ids) {
  for (auto& chunk : m_ChunkAllocations)
    chunk.Flush(ids);
}

void ChunkManager::FlushPreprocessor(VkCommandBuffer commandBuffer) {
  using namespace akari::render;

  if (m_SCO->Dirty())
    m_SCO->Flatten(m_SCONodes);

  m_ChunkState.clear();
  m_ChunkState.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);

  std::vector<Vertex>            vertex;
  std::vector<Buffer::Partition> vertexChunkPartitions;

  std::vector<SparseOctree::FlatNode> svo;
  std::vector<Buffer::Partition>      svoChunkPartitions;
  std::vector<FlatNodeHeader>         svoChunkPartitionHeaders;

  for (auto& chunk : m_ChunkAllocations) {
    bool  dirty = chunk.SVO()->Dirty();
    auto& v     = chunk.Verticies();
    auto& s     = chunk.FlatNodes();

    vertex.insert(vertex.end(), v.begin(), v.end());
    vertexChunkPartitions.emplace_back(chunk.Id, v.size() * sizeof(Vertex), dirty);

    svo.insert(svo.end(), s.begin(), s.end());
    svoChunkPartitions.emplace_back(chunk.Id, s.size() * sizeof(SparseOctree::FlatNode), dirty);
    chunk.SVO()->Clean();
  }

  auto svoAllocations    = m_SVOBuffer.Upload(commandBuffer, svoChunkPartitions, svo.data());
  auto vertexAllocations = m_VertexBuffer.Upload(commandBuffer, vertexChunkPartitions, vertex.data());

  for (size_t i = 0; i < m_ChunkAllocations.size(); i++) {
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

float ChunkManager::TNextBoundary(const glm::vec3& origin, const glm::vec3& direction) {

  const float size = float(SVO_SIZE);

  glm::vec3 nextBoundary = glm::mix(
      glm::vec3(0.0f),
      glm::vec3(size),
      glm::greaterThan(direction, glm::vec3(0.0f)));

  glm::vec3 tVals = (nextBoundary - origin) / direction;

  tVals = glm::mix(
      glm::vec3(1e30f),
      tVals,
      glm::notEqual(direction, glm::vec3(0.0f)));

  return std::min(tVals.x, std::min(tVals.y, tVals.z));
}

} // namespace vxen
