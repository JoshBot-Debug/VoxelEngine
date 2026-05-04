#pragma once

#include <cstdint>
#include <deque>
#include <glm/common.hpp>
#include <glm/glm.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Chunk.h"

#include "voxel/SparseOctree.h"
#include "voxel/Type.h"
#include "voxel/Voxel.h"

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

template <uint32_t SS, uint8_t CS>
class ChunkManager {
  static_assert(SS != 0 && (SS & (SS - 1)) == 0, "SS must be a power of two");
  static_assert(CS != 0 && (CS & (CS - 1)) == 0, "CS must be a power of two");

public:
  static constexpr uint32_t SVO_SIZE {SS};
  static constexpr uint32_t CHUNK_SIZE {CS};

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

private:
  static constexpr uint32_t DIV_SS {std::countr_zero(SS)};
  static constexpr uint32_t MOD_SS {SS - 1};

  akari::render::BufferPool m_SVOPool {};
  akari::render::BufferPool m_SVOHeaderPool {};
  akari::render::BufferPool m_VertexPool {};

  std::deque<Chunk<SS>>                                       m_ChunkAllocator {};
  SparseOctree<Chunk<SS>, CS>*                                m_Chunks {nullptr};
  std::vector<typename SparseOctree<Chunk<SS>, CS>::FlatNode> m_FlatChunkNodes {};
  std::vector<ChunkState>                                     m_ChunkState {};

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
  SparseOctree<Chunk<SS>, CS>::Node* Ensure(uint8_t x, uint8_t y, uint8_t z);

  /**
   * Enures that a chunk will be returned, creates one if it does not exist.
   * @param position The position of the chunk.
   */
  SparseOctree<Chunk<SS>, CS>::Node* Ensure(const glm::u8vec3& position);

public:
  ChunkManager();
  ~ChunkManager();

  ChunkManager(const ChunkManager&)            = delete;
  ChunkManager& operator=(const ChunkManager&) = delete;

  /**
   * Retrieve an RCU {Reader} session.
   */
  SparseOctree<Chunk<SS>, CS>::Reader BeginRead();

  /**
   * Retrieve an RCU {Writer} session.
   */
  SparseOctree<Chunk<SS>, CS>::Writer BeginWrite();

  /**
   * Begin an RCU synchronization.
   */
  void Sync();

  /**
   * @param x,y,z The position of the chunk.
   */
  SparseOctree<Chunk<SS>, CS>::Node* Get(uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param position The position of the chunk.
   */
  SparseOctree<Chunk<SS>, CS>::Node* Get(const glm::u8vec3& position);

  /**
   * @param session The {Reader} session received from BeginRead(...)
   * @param position The position of the chunk.
   */
  SparseOctree<Chunk<SS>, CS>::Node* Get(SparseOctree<Chunk<SS>, CS>::Reader& session, const glm::u8vec3& position);

  /**
   * @param session The {Reader} session received from BeginRead(...)
   * @param x,y,z The position of the chunk.
   */
  SparseOctree<Chunk<SS>, CS>::Node* Get(SparseOctree<Chunk<SS>, CS>::Reader& session, uint8_t x, uint8_t y, uint8_t z);

  /**
   * Flatten the Chunks in to a vector for the GPU
   */
  const std::vector<typename SparseOctree<Chunk<SS>, CS>::FlatNode>& Flatten();

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
  typename SparseOctree<Voxel, SS>::Reader BeginRead(const glm::u8vec3& coordinate);

  /**
   * Retrieve an RCU {Writer} session for the chunk at coordinate.
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   */
  typename SparseOctree<Voxel, SS>::Writer BeginWrite(const glm::u8vec3& coordinate);

  /**
   * Begin an RCU synchronization for the chunk at coordinate.
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   */
  void Sync(const glm::u8vec3& coordinate);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   * @param data The voxel node to insert.
   */
  void Set(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z, Voxel* data);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   * @param data The voxel node to insert.
   */
  void Set(const glm::u8vec3& coordinate, const glm::u8vec3& position, Voxel* data);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Writer} session received from BeginWrite(...)
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   * @param data The voxel node to insert.
   */
  void Set(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, const glm::u8vec3& position, Voxel* data);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Writer} session received from BeginWrite(...)
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   * @param data The voxel node to insert.
   */
  void Set(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z, Voxel* data);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  typename SparseOctree<Voxel, SS>::Node* Get(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   */
  typename SparseOctree<Voxel, SS>::Node* Get(const glm::u8vec3& coordinate, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   */
  typename SparseOctree<Voxel, SS>::Node* Get(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  typename SparseOctree<Voxel, SS>::Node* Get(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, uint8_t x, uint8_t y, uint8_t z);

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
  void Clear(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Writer} session received from BeginWrite(...)
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @return A vector containing the flattened octree for the chunk at coordinate.
   */
  const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& Flatten(const glm::u8vec3& coordinate);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @return A vector containing the flattened octree for the chunk at coordinate.
   */
  const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& Flatten(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param ids The ids of all the voxels to greedy mesh. (SparseOctree requires that the .Data pointer contains an object that has .Id)
   * @return A vector containing the vertices of the octree for the chunk at coordinate.
   */
  const std::vector<Vertex>& GreedyMesh(const glm::u8vec3& coordinate, const std::vector<uint32_t>& ids);

  /**
   * Filters across all chunks
   * @param out The vector in which all filtered nodes will be placed into. Make sure the vector is thread safe.
   * @param filter The filter callback, must return a boolean.
   */
  template <typename F>
    requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
  void Filter(std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param out The vector in which all filtered nodes will be placed into. Make sure the vector is thread safe.
   * @param filter The filter callback, must return a boolean.
   */
  template <typename F>
    requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
  void Filter(const glm::u8vec3& coordinate, std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param out The vector in which all filtered nodes will be placed into. Make sure the vector is thread safe.
   * @param filter The filter callback, must return a boolean.
   */
  template <typename F>
    requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
  void Filter(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter);

  /**
   * @param origin The camera's origin
   * @param direction The camera's direction
   * @return The voxel that was hit
   */
  typename SparseOctree<Voxel, SS>::Hit DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param origin The camera's origin
   * @param direction The camera's direction
   * @return The voxel that was hit
   */
  typename SparseOctree<Voxel, SS>::Hit DeepRaymarch(const glm::u8vec3& coordinate, const glm::vec3& origin, const glm::vec3& direction);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param origin The camera's origin
   * @param direction The camera's direction
   * @return The voxel that was hit
   */
  typename SparseOctree<Voxel, SS>::Hit DeepRaymarch(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, const glm::vec3& origin, const glm::vec3& direction);

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

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Chunk<SS>, CS>::Node* ChunkManager<SS, CS>::Ensure(uint8_t x, uint8_t y, uint8_t z) {
  typename SparseOctree<Chunk<SS>, CS>::Node* chunk = m_Chunks->Get(x, y, z);
  if (chunk)
    return chunk;
  uint32_t index = x + (y * CHUNK_SIZE + (z * CHUNK_SIZE * CHUNK_SIZE));
  m_Chunks->Set(x, y, z, &m_ChunkAllocator.emplace_back(index));
  return m_Chunks->Get(x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Chunk<SS>, CS>::Node* ChunkManager<SS, CS>::Ensure(const glm::u8vec3& position) {
  return Ensure(position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline ChunkManager<SS, CS>::ChunkManager()
    : m_Chunks(new SparseOctree<Chunk<SS>, CS>()) {
  m_SVOBuffer.SetPool(&m_SVOPool);
  m_SVOHeaderBuffer.SetPool(&m_SVOHeaderPool);
  m_VertexBuffer.SetPool(&m_VertexPool);

  m_SVOBuffer.CreateBuffer({.Size = 1024 * 1024 * 1, .DebugName = "m_SVOBuffer"});
  m_SVOHeaderBuffer.CreateBuffer({.Size = 1024 * 1024 * 1, .DebugName = "m_SVOHeaderBuffer"});
  m_VertexBuffer.CreateBuffer({
      .Size      = 1024 * 1024 * 1,
      .Usage     = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .DebugName = "m_VertexBuffer",
  });

  m_ChunkBuffer.CreateBuffer({.DebugName = "m_ChunkBuffer"});
  m_ChunkSVOBuffer.CreateBuffer({.DebugName = "m_ChunkSVOBuffer"});
}

template <uint32_t SS, uint8_t CS>
inline ChunkManager<SS, CS>::~ChunkManager() {
  delete m_Chunks;
}

template <uint32_t SS, uint8_t CS>
inline SparseOctree<Chunk<SS>, CS>::Reader ChunkManager<SS, CS>::BeginRead() {
  return m_Chunks->BeginRead();
}

template <uint32_t SS, uint8_t CS>
inline SparseOctree<Chunk<SS>, CS>::Writer ChunkManager<SS, CS>::BeginWrite() {
  return m_Chunks->BeginWrite();
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Sync() {
  m_Chunks->Sync();
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Chunk<SS>, CS>::Node* ChunkManager<SS, CS>::Get(uint8_t x, uint8_t y, uint8_t z) {
  return m_Chunks->Get(x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Chunk<SS>, CS>::Node* ChunkManager<SS, CS>::Get(const glm::u8vec3& position) {
  return m_Chunks->Get(position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Chunk<SS>, CS>::Node* ChunkManager<SS, CS>::Get(SparseOctree<Chunk<SS>, CS>::Reader& session, const glm::u8vec3& position) {
  return m_Chunks->Get(session, position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Chunk<SS>, CS>::Node* ChunkManager<SS, CS>::Get(SparseOctree<Chunk<SS>, CS>::Reader& session, uint8_t x, uint8_t y, uint8_t z) {
  return m_Chunks->Get(session, x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline const std::vector<typename SparseOctree<Chunk<SS>, CS>::FlatNode>& ChunkManager<SS, CS>::Flatten() {
  m_Chunks->Flatten(m_FlatChunkNodes);
  return m_FlatChunkNodes;
}

template <uint32_t SS, uint8_t CS>
inline ChunkBuffers ChunkManager<SS, CS>::GetBuffers() {
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

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Reader ChunkManager<SS, CS>::BeginRead(const glm::u8vec3& coordinate) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->BeginRead();
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Writer ChunkManager<SS, CS>::BeginWrite(const glm::u8vec3& coordinate) {
  return Ensure(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->BeginWrite();
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Sync(const glm::u8vec3& coordinate) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Sync();
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Set(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z, Voxel* data) {
  auto chunk = Ensure(coordinate.x, coordinate.y, coordinate.z);
  chunk->Data->SVO()->Set(x, y, z, data);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Set(const glm::u8vec3& coordinate, const glm::u8vec3& position, Voxel* data) {
  auto chunk = Ensure(coordinate.x, coordinate.y, coordinate.z);
  chunk->Data->SVO()->Set(position.x, position.y, position.z, data);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Set(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, const glm::u8vec3& position, Voxel* data) {
  auto chunk = Ensure(coordinate.x, coordinate.y, coordinate.z);
  chunk->Data->SVO()->Set(session, position.x, position.y, position.z, data);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Set(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z, Voxel* data) {
  auto chunk = Ensure(coordinate.x, coordinate.y, coordinate.z);
  chunk->Data->SVO()->Set(session, x, y, z, data);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Node* ChunkManager<SS, CS>::Get(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Get(x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Node* ChunkManager<SS, CS>::Get(const glm::u8vec3& coordinate, const glm::u8vec3& position) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Get(position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Node* ChunkManager<SS, CS>::Get(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, const glm::u8vec3& position) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Get(session, position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Node* ChunkManager<SS, CS>::Get(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, uint8_t x, uint8_t y, uint8_t z) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Get(session, x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Clear(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Clear(x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Clear(const glm::u8vec3& coordinate, const glm::u8vec3& position) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Clear(position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Clear(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, const glm::u8vec3& position) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Clear(session, position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Clear(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Clear(session, x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& ChunkManager<SS, CS>::Flatten(const glm::u8vec3& coordinate) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Flatten();
}

template <uint32_t SS, uint8_t CS>
inline const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& ChunkManager<SS, CS>::Flatten(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Flatten(session);
}

template <uint32_t SS, uint8_t CS>
inline const std::vector<Vertex>& ChunkManager<SS, CS>::GreedyMesh(const glm::u8vec3& coordinate, const std::vector<uint32_t>& ids) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->GreedyMesh(coordinate, ids);
}

template <uint32_t SS, uint8_t CS>
template <typename F>
  requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
inline void ChunkManager<SS, CS>::Filter(std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter) {
  m_Chunks->ForEach([&out, &filter](const SparseOctree<Chunk<SS>, CS>::Node* node) {
    std::vector<typename SparseOctree<Voxel, SS>::FilterNode> tmp;
    node->Data->SVO()->Filter(tmp, filter);
    for (auto& n : tmp) {
      n.Position.x += SVO_SIZE * (node->Data->Id % CHUNK_SIZE);
      n.Position.y += SVO_SIZE * ((node->Data->Id / CHUNK_SIZE) % CHUNK_SIZE);
      n.Position.z += SVO_SIZE * (node->Data->Id / (CHUNK_SIZE * CHUNK_SIZE));
    }
    out.insert(out.end(), std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
  });
}

template <uint32_t SS, uint8_t CS>
template <typename F>
  requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
inline void
ChunkManager<SS, CS>::Filter(const glm::u8vec3& coordinate, std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Filter(out, filter);
}

template <uint32_t SS, uint8_t CS>
template <typename F>
  requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
inline void
ChunkManager<SS, CS>::Filter(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->Filter(session, out, filter);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Hit ChunkManager<SS, CS>::DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction) {

  glm::vec3 SIZE = glm::vec3(SVO_SIZE);

  auto check = [&chunks = m_Chunks, &origin, &direction, &SIZE](auto&& self, const glm::vec3& localOrigin, const glm::vec3& coordinate, float tMin) {
    auto chunk = chunks->DeepRaymarch(coordinate, direction);

    if (!chunk.Data)
      return typename SparseOctree<Voxel, SS>::Hit();

    auto hit = chunk.Data->SVO()->DeepRaymarch(localOrigin, direction);
    if (hit.Data) {
      hit.Position.x += SIZE.x * coordinate.x;
      hit.Position.y += SIZE.y * coordinate.y;
      hit.Position.z += SIZE.z * coordinate.z;
      return hit;
    }

    float tStep   = TNextBoundary(localOrigin, direction);
    float tGlobal = tMin + tStep + 1.0f;

    glm::vec3 globalPosition = origin + direction * tGlobal;
    glm::vec3 nCoordinate    = WorldToChunkCoordinateFloat(globalPosition.x, globalPosition.y, globalPosition.z);
    glm::vec3 nLocalOrigin   = glm::mod(globalPosition, SIZE);

    return self(self, nLocalOrigin, nCoordinate, tGlobal);
  };

  glm::vec3 coordinate  = WorldToChunkCoordinateFloat(origin.x, origin.y, origin.z);
  glm::vec3 localOrigin = glm::mod(origin, SIZE);

  return check(check, localOrigin, coordinate, 0.0f);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Hit ChunkManager<SS, CS>::DeepRaymarch(const glm::u8vec3& coordinate, const glm::vec3& origin, const glm::vec3& direction) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->DeepRaymarch(origin, direction);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Hit ChunkManager<SS, CS>::DeepRaymarch(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, const glm::vec3& origin, const glm::vec3& direction) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->SVO()->DeepRaymarch(session, origin, direction);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::FlushVertices(const std::vector<uint32_t>& ids) {
  m_VertexBuffer.Log();
  m_ChunkBuffer.Log();
  m_ChunkSVOBuffer.Log();
  m_SVOBuffer.Log();
  m_SVOHeaderBuffer.Log();

  auto group = akari::thread::ThreadPool::CreateGroup([]() {
    TSignal::Set(0, CHUNK_MANAGER_FLUSH_VERTICES);
  });

  auto session = m_Chunks->BeginRead();

  for (uint8_t z = 0; z < CHUNK_SIZE; z++)
    for (uint8_t y = 0; y < CHUNK_SIZE; y++)
      for (uint8_t x = 0; x < CHUNK_SIZE; x++)
        if (m_Chunks->Exists(x, y, z))
          m_Chunks->Get(session, x, y, z)->Data->Flush(group, {x, y, z}, ids);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::FlushPreprocessor(VkCommandBuffer commandBuffer) {
  m_Chunks->Flatten(m_FlatChunkNodes);

  m_ChunkState.clear();
  m_ChunkState.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);

  std::vector<Vertex>                           vertices;
  std::vector<akari::render::Buffer::Partition> vertexUploads;

  std::vector<typename SparseOctree<Voxel, SS>::FlatNode> flatNodes;
  std::vector<akari::render::Buffer::Partition>           flatNodeUploads;

  std::vector<FlatNodeHeader> flatNodeHeaders;

  for (uint8_t z = 0; z < CHUNK_SIZE; z++)
    for (uint8_t y = 0; y < CHUNK_SIZE; y++)
      for (uint8_t x = 0; x < CHUNK_SIZE; x++)
        if (m_Chunks->Exists(x, y, z)) {
          Chunk<SS>* chunk = m_Chunks->Get(x, y, z)->Data;
          uint32_t   id    = chunk->Id;
          auto&      v     = chunk->Verticies();
          auto&      fn    = chunk->FlatNodes();

          vertices.insert(vertices.end(), v.begin(), v.end());
          vertexUploads.emplace_back(id, v.size() * sizeof(Vertex));

          flatNodes.insert(flatNodes.end(), fn.begin(), fn.end());
          flatNodeUploads.emplace_back(id, fn.size() * sizeof(typename SparseOctree<Voxel, SS>::FlatNode));
        }

  auto svoAllocations = m_SVOBuffer.Upload(commandBuffer, flatNodeUploads, flatNodes.data());

  auto vertexAllocations = m_VertexBuffer.Upload(commandBuffer, vertexUploads, vertices.data());

  for (auto& alloc : vertexAllocations) {
    m_ChunkState[alloc.Id].Empty     = 0;
    m_ChunkState[alloc.Id].Offset[0] = alloc.Offset / sizeof(Vertex);
    m_ChunkState[alloc.Id].Size[0]   = alloc.Size / sizeof(Vertex);
  }

  /// TODO: The id mostly won't sync with the index. Need to sort so that ids go from 0 - whatever. Id is index is chunk scalar coordinate location
  for (auto& alloc : svoAllocations) {
    uint32_t offset = alloc.Offset / sizeof(typename SparseOctree<Voxel, SS>::FlatNode);
    uint32_t size   = alloc.Size / sizeof(typename SparseOctree<Voxel, SS>::FlatNode);
    flatNodeHeaders.emplace_back(alloc.Id, offset, size);
  }

  m_SVOHeaderBuffer.Upload(commandBuffer, flatNodeHeaders);
  m_ChunkSVOBuffer.Upload(commandBuffer, m_FlatChunkNodes);
  m_ChunkBuffer.Upload(commandBuffer, m_ChunkState);
}

template <uint32_t SS, uint8_t CS>
inline glm::u8vec3 ChunkManager<SS, CS>::WorldToChunkCoordinate(float x, float y, float z) {
  auto floor = [size = SS, div = DIV_SS](int a) {
    return (a >= 0) ? (a >> div) : ((a - size + 1) >> div);
  };
  return glm::u8vec3(floor(static_cast<int>(x)), floor(static_cast<int>(y)), floor(static_cast<int>(z)));
}

template <uint32_t SS, uint8_t CS>
inline glm::u8vec3 ChunkManager<SS, CS>::WorldToChunkCoordinate(const glm::vec3& worldPosition) {
  return WorldToChunkCoordinate(worldPosition.x, worldPosition.y, worldPosition.z);
}

template <uint32_t SS, uint8_t CS>
inline glm::vec3 ChunkManager<SS, CS>::WorldToChunkCoordinateFloat(float x, float y, float z) {
  auto floor = [size = SS](float a) {
    return uint32_t((a >= 0) ? (a / size) : ((a - size + 1) / size));
  };
  return glm::vec3(floor(x), floor(y), floor(z));
}

template <uint32_t SS, uint8_t CS>
inline glm::vec3 ChunkManager<SS, CS>::WorldToChunkCoordinateFloat(const glm::vec3& worldPosition) {
  return WorldToChunkCoordinateFloat(worldPosition.x, worldPosition.y, worldPosition.z);
}

template <uint32_t SS, uint8_t CS>
inline glm::u8vec3 ChunkManager<SS, CS>::WorldToLocalCoordinate(float x, float y, float z) {
  x = std::fmod(x, SS);
  y = std::fmod(y, SS);
  z = std::fmod(z, SS);
  return glm::u8vec3(static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(z));
}

template <uint32_t SS, uint8_t CS>
inline glm::u8vec3 ChunkManager<SS, CS>::WorldToLocalCoordinate(const glm::vec3& worldPosition) {
  return WorldToLocalCoordinate(worldPosition.x, worldPosition.y, worldPosition.z);
}

template <uint32_t SS, uint8_t CS>
inline uint8_t ChunkManager<SS, CS>::Wrap(int i) {
  int m = i & MOD_SS;
  return (m < 0) ? m + int(SS) : m;
}

template <uint32_t SS, uint8_t CS>
inline float ChunkManager<SS, CS>::TNextBoundary(const glm::vec3& localOrigin, const glm::vec3& direction) {
  glm::vec3 inverseDirection = 1.0f / direction;

  float size = float(SVO_SIZE);

  // Compute base cell
  glm::vec3 cell = glm::floor(localOrigin / size) * size;

  // Select next boundary using step (1 for +, 0 for -)
  glm::vec3 stepDir      = glm::step(glm::vec3(0.0f), direction);
  glm::vec3 nextBoundary = cell + stepDir * size;

  // Compute t values
  glm::vec3 tVals = (nextBoundary - localOrigin) * inverseDirection;

  // Handle zero directions (branchless)
  glm::bvec3 mask = glm::notEqual(direction, glm::vec3(0.0f));
  tVals           = glm::mix(glm::vec3(1e30f), tVals, mask);

  return std::min(tVals.x, std::min(tVals.y, tVals.z));
}

} // namespace vxen
