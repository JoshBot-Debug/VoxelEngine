#pragma once

#include <cstdint>
#include <deque>
#include <glm/common.hpp>
#include <glm/glm.hpp>

#include <vk_mem_alloc.h>

#include "Chunk.h"

#include "voxel/SparseOctree.h"
#include "voxel/Type.h"
#include "voxel/Voxel.h"

#include "Debug.h"
#include "Signal.h"
#include "render/BufferPool.h"
#include "thread/ThreadPool.h"

namespace vxen {

struct ChunkBuffers {
  VkBuffer SVOBuffer       = VK_NULL_HANDLE;
  VkBuffer SVOHeaderBuffer = VK_NULL_HANDLE;
  VkBuffer VertexBuffer    = VK_NULL_HANDLE;
  VkBuffer ChunkSVOBuffer  = VK_NULL_HANDLE;
  VkBuffer ChunkBuffer     = VK_NULL_HANDLE;

  VkBufferMemoryBarrier2 SVOBufferBarrier;
  VkBufferMemoryBarrier2 SVOHeaderBufferBarrier;
  VkBufferMemoryBarrier2 VertexBufferBarrier;
  VkBufferMemoryBarrier2 ChunkSVOBufferBarrier;
  VkBufferMemoryBarrier2 ChunkBufferBarrier;
};

class ChunkManager {
public:
  using ChunkNode = Node;

public:
  static constexpr uint32_t SVO_SIZE {64};
  static constexpr uint32_t CHUNK_SIZE {64};

  struct ChunkState {
    uint32_t Empty {1};
    uint32_t Offset[6] {0};
    uint32_t Size[6] {0};
  };

  struct FlatNodeHeader {
    uint32_t Id;
    uint32_t Offset;
    uint32_t Size;
  };

  struct RaymarchRay {
    glm::vec4 origin;
    glm::vec4 direction;
  };

private:
  static constexpr uint32_t DIV_SS {std::countr_zero(64U)};
  static constexpr uint32_t MOD_SS {64U - 1};

  akari::render::BufferPool m_SVOPool {};
  akari::render::BufferPool m_SVOHeaderPool {};
  akari::render::BufferPool m_VertexPool {};

  /**
   * Keeps the Chunk allocations, [index + 1] is used as the ID for SCO
   */
  std::deque<Chunk> m_ChunkAllocations {};

  /**
   * SCO
   * Sparse Chunk Octree
   */
  SparseOctree*                       m_SCO {nullptr};
  std::vector<SparseOctree::FlatNode> m_SCONodes {};
  std::vector<ChunkState>             m_ChunkState {};

  akari::render::Buffer m_SVOBuffer {};
  akari::render::Buffer m_SVOHeaderBuffer {};
  akari::render::Buffer m_VertexBuffer {};

  akari::render::Buffer m_ChunkSVOBuffer {};
  akari::render::Buffer m_ChunkBuffer {};

private:
  /**
   * Enures that a chunk will be returned, creates one if it does not exist.
   * @param x,y,z The position of the chunk.
   */
  ChunkNode* Ensure(uint8_t x, uint8_t y, uint8_t z);

  /**
   * Enures that a chunk will be returned, creates one if it does not exist.
   * @param position The position of the chunk.
   */
  ChunkNode* Ensure(const glm::u8vec3& position);

public:
  ChunkManager();
  ~ChunkManager();

  ChunkManager(const ChunkManager&)            = delete;
  ChunkManager& operator=(const ChunkManager&) = delete;

  /**
   * Retrieve an RCU {Reader} session.
   */
  SparseOctree::Reader BeginRead();

  /**
   * Retrieve an RCU {Writer} session.
   */
  SparseOctree::Writer BeginWrite();

  /**
   * Begin an RCU synchronization.
   */
  void Sync();

  /**
   * @param x,y,z The position of the chunk.
   */
  ChunkNode* Get(uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param position The position of the chunk.
   */
  ChunkNode* Get(const glm::u8vec3& position);

  /**
   * @param session The {Reader} session received from BeginRead(...)
   * @param position The position of the chunk.
   */
  ChunkNode* Get(SparseOctree::Reader& session, const glm::u8vec3& position);

  /**
   * @param session The {Reader} session received from BeginRead(...)
   * @param x,y,z The position of the chunk.
   */
  ChunkNode* Get(SparseOctree::Reader& session, uint8_t x, uint8_t y, uint8_t z);

  /**
   * Flatten the Chunks in to a vector for the GPU
   */
  const std::vector<SparseOctree::FlatNode>& Flatten();

  /**
   * Get all buffers
   */
  ChunkBuffers GetBuffers();

  /**
   * ******************************************************************************
   * NOTE:                                                                        *
   * Every method below this point is for accessing SVO's in chunks and hence all *
   * require you to pass in a coordinate.                                         *
   *                                                                              *
   * ******************************************************************************
   */

  /**
   * Retrieve an RCU {Reader} session for the chunk at coordinate.
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   */
  SparseOctree::Reader BeginRead(const glm::u8vec3& coordinate);

  /**
   * Retrieve an RCU {Writer} session for the chunk at coordinate.
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   */
  SparseOctree::Writer BeginWrite(const glm::u8vec3& coordinate);

  /**
   * Begin an RCU synchronization for the chunk at coordinate.
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   */
  void Sync(const glm::u8vec3& coordinate);

  /**
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   * @param id The voxel node to insert.
   */
  void Set(uint8_t x, uint8_t y, uint8_t z, uint32_t id);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   * @param id The voxel node to insert.
   */
  void Set(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z, uint32_t id);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Writer} session received from BeginWrite(...)
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   * @param id The voxel node to insert.
   */
  void Set(const glm::u8vec3& coordinate, SparseOctree::Writer& session, uint8_t x, uint8_t y, uint8_t z, uint32_t id);

  /**
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   * @param id The voxel node to insert.
   */
  void Set(const glm::u8vec3& position, uint32_t id);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   * @param id The voxel node to insert.
   */
  void Set(const glm::u8vec3& coordinate, const glm::u8vec3& position, uint32_t id);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Writer} session received from BeginWrite(...)
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   * @param id The voxel node to insert.
   */
  void Set(const glm::u8vec3& coordinate, SparseOctree::Writer& session, const glm::u8vec3& position, uint32_t id);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  Node* Get(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   */
  Node* Get(const glm::u8vec3& coordinate, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   */
  Node* Get(const glm::u8vec3& coordinate, SparseOctree::Reader& session, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  Node* Get(const glm::u8vec3& coordinate, SparseOctree::Reader& session, uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param session The {Writer} session received from BeginWrite(...)
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(SparseOctree::Writer& session, uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(const glm::u8vec3& position);

  /**
   * @param session The {Writer} session received from BeginWrite(...)
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(SparseOctree::Writer& session, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(const glm::u8vec3& coordinate, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Writer} session received from BeginWrite(...)
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(const glm::u8vec3& coordinate, SparseOctree::Writer& session, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Writer} session received from BeginWrite(...)
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(const glm::u8vec3& coordinate, SparseOctree::Writer& session, uint8_t x, uint8_t y, uint8_t z);

  /**
   * Filters across all chunks
   * @param out The vector in which all filtered nodes will be placed into. Make sure the vector is thread safe.
   * @param filter The filter callback, must return a boolean.
   */
  template <typename F>
    requires FilterCallback<Node, F>
  void Filter(std::vector<SparseOctree::FilterNode>& out, F&& filter);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param out The vector in which all filtered nodes will be placed into. Make sure the vector is thread safe.
   * @param filter The filter callback, must return a boolean.
   */
  template <typename F>
    requires FilterCallback<Node, F>
  void Filter(const glm::u8vec3& coordinate, std::vector<SparseOctree::FilterNode>& out, F&& filter);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param out The vector in which all filtered nodes will be placed into. Make sure the vector is thread safe.
   * @param filter The filter callback, must return a boolean.
   */
  template <typename F>
    requires FilterCallback<Node, F>
  void Filter(const glm::u8vec3& coordinate, SparseOctree::Reader& session, std::vector<SparseOctree::FilterNode>& out, F&& filter);

  /**
   * @param origin The camera's origin
   * @param direction The camera's direction
   * @return The voxel that was hit
   */
  SparseOctree::Hit DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction);

  /**
   * @param origin The camera's origin
   * @param direction The camera's direction
   * @return The voxel that was hit
   */
  SparseOctree::Hit Raymarch(const glm::vec3& origin, const glm::vec3& direction);

  /**
   * Triggers multi thread greedy meshing.
   * @param ids Used to split greedy meshing, each id is meshed in a seperate thread. Generally these ids would represent different voxel types, each type can be merged together.
   */
  void FlushVertices(const std::vector<uint32_t>& ids);

  /**
   * Collects all flushed chunks that are dirty
   * @param commandBuffer The command buffer for this frame
   * @returns A vector of flushed chunks that need to be rendered. Uses to create indirect draw commands.
   */
  void FlushPreprocessor(VkCommandBuffer commandBuffer);

  /**
   * Converts world coordinates (96, 32, 32) to chunk coordinates (1, 0, 0)
   */
  static inline glm::u8vec3 WorldToChunkCoordinate(float x, float y, float z);

  /**
   * Converts world coordinates (96, 32, 32) to chunk coordinates (1, 0, 0)
   */
  static inline glm::u8vec3 WorldToChunkCoordinate(const glm::vec3& worldPosition);

  /**
   * Converts world coordinates (96, 32, 32) to chunk coordinates (1.5f, 0.0f, 0.0f)
   */
  static inline glm::vec3 WorldToChunkCoordinateFloat(float x, float y, float z);

  /**
   * Converts world coordinates (96, 32, 32) to chunk coordinates (1.5f, 0.0f, 0.0f)
   */
  static inline glm::vec3 WorldToChunkCoordinateFloat(const glm::vec3& worldPosition);

  /**
   * Converts world coordinates (96, 32, 32) to local coordinates (32, 32, 32)
   */
  static inline glm::u8vec3 WorldToLocalCoordinate(float x, float y, float z);

  /**
   * Converts world coordinates (96, 32, 32) to local coordinates (32, 32, 32)
   */
  static inline glm::u8vec3 WorldToLocalCoordinate(const glm::vec3& worldPosition);

  /**
   * Wraps to the previous chunk coordinate.
   *  i.e -1 --> 63
   *  i.e 64 --> 0
   *
   * @note Does nothing for -2..., 65... This is intended because so far there is no reason to handle it.
   */
  static inline uint8_t Wrap(int i);

  /**
   * Move the origin to the boundary of the next chunk
   */
  static inline float TNextBoundary(const glm::vec3& localOrigin, const glm::vec3& direction);
};

inline ChunkManager::ChunkNode* ChunkManager::Ensure(uint8_t x, uint8_t y, uint8_t z) {
  ChunkManager::ChunkNode* chunk = m_SCO->Get(x, y, z);
  if (chunk)
    return chunk;
  uint32_t cid = m_ChunkAllocations.size() + 1;
  m_ChunkAllocations.emplace_back(cid, glm::u8vec3 {x, y, z});
  m_SCO->Set(x, y, z, cid);
  return m_SCO->Get(x, y, z);
}

inline ChunkManager::ChunkNode* ChunkManager::Ensure(const glm::u8vec3& position) {
  return Ensure(position.x, position.y, position.z);
}

inline ChunkManager::ChunkManager()
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

inline ChunkManager::~ChunkManager() {
  delete m_SCO;
}

inline SparseOctree::Reader ChunkManager::BeginRead() {
  return m_SCO->BeginRead();
}

inline SparseOctree::Writer ChunkManager::BeginWrite() {
  return m_SCO->BeginWrite();
}

inline void ChunkManager::Sync() {
  m_SCO->Sync();
}

inline ChunkManager::ChunkNode* ChunkManager::Get(uint8_t x, uint8_t y, uint8_t z) {
  return m_SCO->Get(x, y, z);
}

inline ChunkManager::ChunkNode* ChunkManager::Get(const glm::u8vec3& position) {
  return m_SCO->Get(position.x, position.y, position.z);
}

inline ChunkManager::ChunkNode* ChunkManager::Get(SparseOctree::Reader& session, const glm::u8vec3& position) {
  return m_SCO->Get(session, position.x, position.y, position.z);
}

inline ChunkManager::ChunkNode* ChunkManager::Get(SparseOctree::Reader& session, uint8_t x, uint8_t y, uint8_t z) {
  return m_SCO->Get(session, x, y, z);
}

inline const std::vector<SparseOctree::FlatNode>& ChunkManager::Flatten() {
  m_SCO->Flatten(m_SCONodes);
  return m_SCONodes;
}

inline ChunkBuffers ChunkManager::GetBuffers() {
  return {
      .SVOBuffer              = m_SVOBuffer.GetBuffer(),
      .SVOHeaderBuffer        = m_SVOHeaderBuffer.GetBuffer(),
      .VertexBuffer           = m_VertexBuffer.GetBuffer(),
      .ChunkSVOBuffer         = m_ChunkSVOBuffer.GetBuffer(),
      .ChunkBuffer            = m_ChunkBuffer.GetBuffer(),
      .SVOBufferBarrier       = m_SVOBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT),
      .SVOHeaderBufferBarrier = m_SVOHeaderBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT),
      .VertexBufferBarrier    = m_VertexBuffer.GetBarrier(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT),
      .ChunkSVOBufferBarrier  = m_ChunkSVOBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT),
      .ChunkBufferBarrier     = m_ChunkBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT),
  };
}

inline SparseOctree::Reader ChunkManager::BeginRead(const glm::u8vec3& coordinate) {
  uint32_t chunkId = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  return m_ChunkAllocations[chunkId].SVO()->BeginRead();
}

inline SparseOctree::Writer ChunkManager::BeginWrite(const glm::u8vec3& coordinate) {
  auto     chunk   = Ensure(coordinate.x, coordinate.y, coordinate.z);
  uint32_t chunkId = chunk->Id - 1;
  return m_ChunkAllocations[chunkId].SVO()->BeginWrite();
}

inline void ChunkManager::Sync(const glm::u8vec3& coordinate) {
  uint32_t chunkId = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Sync();
}

inline void ChunkManager::Set(uint8_t x, uint8_t y, uint8_t z, uint32_t id) {
  const glm::u8vec3 coordinate {x / SVO_SIZE, y / SVO_SIZE, z / SVO_SIZE};
  auto              chunkId = Ensure(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Set(x % SVO_SIZE, y % SVO_SIZE, z % SVO_SIZE, id);
}

inline void ChunkManager::Set(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z, uint32_t id) {
  auto chunkId = Ensure(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Set(x, y, z, id);
}

inline void ChunkManager::Set(const glm::u8vec3& coordinate, SparseOctree::Writer& session, uint8_t x, uint8_t y, uint8_t z, uint32_t id) {
  auto chunkId = Ensure(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Set(session, x, y, z, id);
}

inline void ChunkManager::Set(const glm::u8vec3& coordinate, const glm::u8vec3& position, uint32_t id) {
  Set(coordinate, position.x, position.y, position.z, id);
}

inline void ChunkManager::Set(const glm::u8vec3& position, uint32_t id) {
  Set(position.x, position.y, position.z, id);
}

inline void ChunkManager::Set(const glm::u8vec3& coordinate, SparseOctree::Writer& session, const glm::u8vec3& position, uint32_t id) {
  Set(coordinate, session, position.x, position.y, position.z, id);
}

inline Node* ChunkManager::Get(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z) {
  uint32_t chunkId = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  return m_ChunkAllocations[chunkId].SVO()->Get(x, y, z);
}

inline Node* ChunkManager::Get(const glm::u8vec3& coordinate, const glm::u8vec3& position) {
  uint32_t chunkId = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  return m_ChunkAllocations[chunkId].SVO()->Get(position.x, position.y, position.z);
}

inline Node* ChunkManager::Get(const glm::u8vec3& coordinate, SparseOctree::Reader& session, const glm::u8vec3& position) {
  uint32_t chunkId = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  return m_ChunkAllocations[chunkId].SVO()->Get(session, position.x, position.y, position.z);
}

inline Node* ChunkManager::Get(const glm::u8vec3& coordinate, SparseOctree::Reader& session, uint8_t x, uint8_t y, uint8_t z) {
  uint32_t chunkId = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  return m_ChunkAllocations[chunkId].SVO()->Get(session, x, y, z);
}

inline void ChunkManager::Clear(uint8_t x, uint8_t y, uint8_t z) {
  const glm::u8vec3 coordinate {x / SVO_SIZE, y / SVO_SIZE, z / SVO_SIZE};
  auto              chunkNode = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  if (!chunkNode)
    return;
  uint32_t chunkId = chunkNode->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Clear(x % SVO_SIZE, y % SVO_SIZE, z % SVO_SIZE);
}

inline void ChunkManager::Clear(SparseOctree::Writer& session, uint8_t x, uint8_t y, uint8_t z) {
  const glm::u8vec3 coordinate {x / SVO_SIZE, y / SVO_SIZE, z / SVO_SIZE};
  auto              chunkNode = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  if (!chunkNode)
    return;
  uint32_t chunkId = chunkNode->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Clear(session, x % SVO_SIZE, y % SVO_SIZE, z % SVO_SIZE);
}

inline void ChunkManager::Clear(const glm::u8vec3& position) {
  Clear(position.x, position.y, position.z);
}

inline void ChunkManager::Clear(SparseOctree::Writer& session, const glm::u8vec3& position) {
  Clear(session, position.x, position.y, position.z);
}

inline void ChunkManager::Clear(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z) {
  auto chunkNode = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  if (!chunkNode)
    return;
  uint32_t chunkId = chunkNode->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Clear(x, y, z);
}

inline void ChunkManager::Clear(const glm::u8vec3& coordinate, const glm::u8vec3& position) {
  Clear(coordinate, position.x, position.y, position.z);
}

inline void ChunkManager::Clear(const glm::u8vec3& coordinate, SparseOctree::Writer& session, uint8_t x, uint8_t y, uint8_t z) {
  auto chunkNode = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  if (!chunkNode)
    return;
  uint32_t chunkId = chunkNode->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Clear(session, x, y, z);
}

inline void ChunkManager::Clear(const glm::u8vec3& coordinate, SparseOctree::Writer& session, const glm::u8vec3& position) {
  Clear(coordinate, session, position.x, position.y, position.z);
}

template <typename F>
  requires FilterCallback<Node, F>
inline void ChunkManager::Filter(std::vector<SparseOctree::FilterNode>& out, F&& filter) {
  m_SCO->ForEach([&out, &filter, &chunkAllocator = m_ChunkAllocations](const ChunkNode* node) {
    std::vector<SparseOctree::FilterNode> tmp;
    uint32_t                              chunkIndex = node->Id - 1;
    vxen::Chunk&                          chunk      = chunkAllocator[chunkIndex];
    chunk.SVO()->Filter(tmp, filter);
    for (auto& n : tmp) {
      auto offset = chunk.GetCoordinate();
      n.Position.x += (offset.x * CHUNK_SIZE) + (SVO_SIZE * (chunkIndex % CHUNK_SIZE));
      n.Position.y += (offset.y * CHUNK_SIZE) + (SVO_SIZE * ((chunkIndex / CHUNK_SIZE) % CHUNK_SIZE));
      n.Position.z += (offset.z * CHUNK_SIZE) + (SVO_SIZE * (chunkIndex / (CHUNK_SIZE * CHUNK_SIZE)));
    }
    out.insert(out.end(), std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
  });
}

template <typename F>
  requires FilterCallback<Node, F>
inline void
ChunkManager::Filter(const glm::u8vec3& coordinate, std::vector<SparseOctree::FilterNode>& out, F&& filter) {
  uint32_t chunkId = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Filter(out, filter);
}

template <typename F>
  requires FilterCallback<Node, F>
inline void
ChunkManager::Filter(const glm::u8vec3& coordinate, SparseOctree::Reader& session, std::vector<SparseOctree::FilterNode>& out, F&& filter) {
  uint32_t chunkId = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z)->Id - 1;
  m_ChunkAllocations[chunkId].SVO()->Filter(session, out, filter);
}

inline SparseOctree::Hit ChunkManager::DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction) {

  glm::vec3 SIZE = glm::vec3(SVO_SIZE);

  auto check = [&chunks = m_SCO, &origin, &direction, &SIZE, &chunkAllocator = m_ChunkAllocations](auto&& self, const glm::vec3& localOrigin, const glm::vec3& coordinate, float tMin) {
    auto chunk = chunks->DeepRaymarch(coordinate, direction);

    if (!chunk.Id)
      return SparseOctree::Hit();

    auto hit = chunkAllocator[chunk.Id - 1].SVO()->DeepRaymarch(localOrigin, direction);
    if (hit.Id) {
      hit.Position.x += SIZE.x * coordinate.x;
      hit.Position.y += SIZE.y * coordinate.y;
      hit.Position.z += SIZE.z * coordinate.z;
      return hit;
    }

    float tStep   = TNextBoundary(localOrigin, direction);
    float tGlobal = tMin + tStep + 0.001f;

    glm::vec3 globalPosition = origin + direction * tGlobal;
    glm::vec3 nCoordinate    = WorldToChunkCoordinateFloat(globalPosition.x, globalPosition.y, globalPosition.z);
    glm::vec3 nLocalOrigin   = glm::mod(globalPosition, SIZE);

    return self(self, nLocalOrigin, nCoordinate, tGlobal);
  };

  glm::vec3 coordinate  = WorldToChunkCoordinateFloat(origin.x, origin.y, origin.z);
  glm::vec3 localOrigin = glm::mod(origin, SIZE);

  return check(check, localOrigin, coordinate, 0.0f);
}

inline SparseOctree::Hit ChunkManager::Raymarch(const glm::vec3& origin, const glm::vec3& direction) {
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

inline void ChunkManager::FlushVertices(const std::vector<uint32_t>& ids) {
  // m_VertexBuffer.Log();
  // m_ChunkBuffer.Log();
  // m_ChunkSVOBuffer.Log();
  // m_SVOBuffer.Log();
  // m_SVOHeaderBuffer.Log();

  auto session = m_SCO->BeginRead();

  for (uint8_t z = 0; z < CHUNK_SIZE; z++)
    for (uint8_t y = 0; y < CHUNK_SIZE; y++)
      for (uint8_t x = 0; x < CHUNK_SIZE; x++)
        if (m_SCO->Exists(x, y, z)) {
          uint32_t chunkId = m_SCO->Get(session, x, y, z)->Id - 1;
          m_ChunkAllocations[chunkId].Flush({x, y, z}, ids);
        }

  TSignal::Set(0, CHUNK_MANAGER_FLUSH_VERTICES);
}

inline void ChunkManager::FlushPreprocessor(VkCommandBuffer commandBuffer) {
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

  const size_t AllocatedChunksCount = m_ChunkAllocations.size();

  for (size_t i = 0; i < AllocatedChunksCount; i++) {
    Chunk& chunk = m_ChunkAllocations[i];

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

  for (size_t i = 0; i < AllocatedChunksCount; i++) {
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

inline glm::u8vec3 ChunkManager::WorldToChunkCoordinate(float x, float y, float z) {
  auto floor = [size = 64, div = DIV_SS](int a) {
    return (a >= 0) ? (a >> div) : ((a - size + 1) >> div);
  };
  return glm::u8vec3(floor(static_cast<int>(x)), floor(static_cast<int>(y)), floor(static_cast<int>(z)));
}

inline glm::u8vec3 ChunkManager::WorldToChunkCoordinate(const glm::vec3& worldPosition) {
  return WorldToChunkCoordinate(worldPosition.x, worldPosition.y, worldPosition.z);
}

inline glm::vec3 ChunkManager::WorldToChunkCoordinateFloat(float x, float y, float z) {
  auto floor = [size = 64](float a) {
    return uint32_t((a >= 0) ? (a / size) : ((a - size + 1) / size));
  };
  return glm::vec3(floor(x), floor(y), floor(z));
}

inline glm::vec3 ChunkManager::WorldToChunkCoordinateFloat(const glm::vec3& worldPosition) {
  return WorldToChunkCoordinateFloat(worldPosition.x, worldPosition.y, worldPosition.z);
}

inline glm::u8vec3 ChunkManager::WorldToLocalCoordinate(float x, float y, float z) {
  x = std::fmod(x, 64);
  y = std::fmod(y, 64);
  z = std::fmod(z, 64);
  return glm::u8vec3(static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(z));
}

inline glm::u8vec3 ChunkManager::WorldToLocalCoordinate(const glm::vec3& worldPosition) {
  return WorldToLocalCoordinate(worldPosition.x, worldPosition.y, worldPosition.z);
}

inline uint8_t ChunkManager::Wrap(int i) {
  int m = i & MOD_SS;
  return (m < 0) ? m + int(64) : m;
}

inline float ChunkManager::TNextBoundary(const glm::vec3& origin, const glm::vec3& direction) {

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
