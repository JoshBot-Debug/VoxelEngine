#pragma once

#include <cstdint>
#include <deque>
#include <glm/glm.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Chunk.h"

#include "voxel/SparseOctree.h"
#include "voxel/Type.h"
#include "voxel/Voxel.h"

#include "render/BufferPool.h"
#include "state/StateMachine.h"
#include "window/Application.h"

namespace vxen {

template <uint32_t SS, uint8_t CS>
class ChunkManager {
  static_assert(SS != 0 && (SS & (SS - 1)) == 0, "SS must be a power of two");
  static_assert(CS != 0 && (CS & (CS - 1)) == 0, "CS must be a power of two");

public:
  static constexpr uint32_t SVO_SIZE {SS};
  static constexpr uint32_t CHUNK_SIZE {CS};

private:
  static constexpr uint32_t DIV_SS {std::countr_zero(SS)};
  static constexpr uint32_t MOD_SS {SS - 1};

  akari::render::BufferPool m_SVOPool {};
  akari::render::BufferPool m_VertexPool {};

  std::deque<Chunk<SS>>                         m_ChunkAllocator {};
  SparseOctree<Chunk<SS>, CS>*                  m_Chunks {nullptr};
  std::vector<typename Chunk<SS>::FlushedChunk> m_FlushedChunks {};

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
   * @param session The {Reader} session received from BeginRead(...)
   * @param origin The camera's origin
   * @param direction The camera's direction
   * @return The voxel that was hit
   */
  typename SparseOctree<Voxel, SS>::Hit DeepRaymarch(typename SparseOctree<Voxel, SS>::Reader& session, const glm::vec3& origin, const glm::vec3& direction);

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

  void FlushUpdates(const std::vector<uint32_t>& ids);

  const std::vector<typename Chunk<SS>::FlushedChunk>& FlushRenderer(VkCommandBuffer commandbuffer);

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
};

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Chunk<SS>, CS>::Node* ChunkManager<SS, CS>::Ensure(uint8_t x, uint8_t y, uint8_t z) {
  typename SparseOctree<Chunk<SS>, CS>::Node* chunk = m_Chunks->Get(x, y, z);
  if (chunk)
    return chunk;
  m_Chunks->Set(x, y, z, &m_ChunkAllocator.emplace_back(&m_SVOPool, &m_VertexPool));
  return m_Chunks->Get(x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Chunk<SS>, CS>::Node* ChunkManager<SS, CS>::Ensure(const glm::u8vec3& position) {
  return Ensure(position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline ChunkManager<SS, CS>::ChunkManager()
    : m_Chunks(new SparseOctree<Chunk<SS>, CS>()) {}

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
inline typename SparseOctree<Voxel, SS>::Reader ChunkManager<SS, CS>::BeginRead(const glm::u8vec3& coordinate) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->BeginRead();
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Writer ChunkManager<SS, CS>::BeginWrite(const glm::u8vec3& coordinate) {
  return Ensure(coordinate.x, coordinate.y, coordinate.z)->Data->BeginWrite();
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Sync(const glm::u8vec3& coordinate) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Sync();
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Set(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z, Voxel* data) {
  Ensure(coordinate.x, coordinate.y, coordinate.z)->Data->Set(x, y, z, data);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Set(const glm::u8vec3& coordinate, const glm::u8vec3& position, Voxel* data) {
  Ensure(coordinate.x, coordinate.y, coordinate.z)->Data->Set(position.x, position.y, position.z, data);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Set(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, const glm::u8vec3& position, Voxel* data) {
  Ensure(coordinate.x, coordinate.y, coordinate.z)->Data->Set(session, position.x, position.y, position.z, data);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Set(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z, Voxel* data) {
  Ensure(coordinate.x, coordinate.y, coordinate.z)->Data->Set(session, x, y, z, data);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Node* ChunkManager<SS, CS>::Get(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Get(x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Node* ChunkManager<SS, CS>::Get(const glm::u8vec3& coordinate, const glm::u8vec3& position) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Get(position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Node* ChunkManager<SS, CS>::Get(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, const glm::u8vec3& position) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Get(session, position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Node* ChunkManager<SS, CS>::Get(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, uint8_t x, uint8_t y, uint8_t z) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Get(session, x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Clear(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Clear(x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Clear(const glm::u8vec3& coordinate, const glm::u8vec3& position) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Clear(position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Clear(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, const glm::u8vec3& position) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Clear(session, position.x, position.y, position.z);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Clear(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Writer& session, uint8_t x, uint8_t y, uint8_t z) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Clear(session, x, y, z);
}

template <uint32_t SS, uint8_t CS>
inline const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& ChunkManager<SS, CS>::Flatten(const glm::u8vec3& coordinate) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Flatten();
}

template <uint32_t SS, uint8_t CS>
inline const std::vector<typename SparseOctree<Voxel, SS>::FlatNode>& ChunkManager<SS, CS>::Flatten(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Flatten(session);
}

template <uint32_t SS, uint8_t CS>
inline const std::vector<Vertex>& ChunkManager<SS, CS>::GreedyMesh(const glm::u8vec3& coordinate, const std::vector<uint32_t>& ids) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->GreedyMesh(coordinate, ids);
}

template <uint32_t SS, uint8_t CS>
template <typename F>
  requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
inline void
ChunkManager<SS, CS>::Filter(const glm::u8vec3& coordinate, std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Filter(out, filter);
}

template <uint32_t SS, uint8_t CS>
template <typename F>
  requires FilterCallback<typename SparseOctree<Voxel, SS>::Node, F>
inline void
ChunkManager<SS, CS>::Filter(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, std::vector<typename SparseOctree<Voxel, SS>::FilterNode>& out, F&& filter) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->Filter(session, out, filter);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Hit ChunkManager<SS, CS>::DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction) {
  glm::vec3                                 coordinate = WorldToChunkCoordinateFloat(origin.x, origin.y, origin.z);
  typename SparseOctree<Chunk<SS>, CS>::Hit chunk      = m_Chunks->DeepRaymarch(coordinate, direction);
  if (!chunk.Data)
    return typename SparseOctree<Voxel, SS>::Hit();
  return chunk.Data->DeepRaymarch(origin, direction);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Hit ChunkManager<SS, CS>::DeepRaymarch(typename SparseOctree<Voxel, SS>::Reader& session, const glm::vec3& origin, const glm::vec3& direction) {
  glm::vec3                                 coordinate = WorldToChunkCoordinateFloat(origin.x, origin.y, origin.z);
  typename SparseOctree<Chunk<SS>, CS>::Hit chunk      = m_Chunks->DeepRaymarch(coordinate, direction);
  if (!chunk.Data)
    return typename SparseOctree<Voxel, SS>::Hit();
  return chunk.Data->DeepRaymarch(session, origin, direction);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Hit ChunkManager<SS, CS>::DeepRaymarch(const glm::u8vec3& coordinate, const glm::vec3& origin, const glm::vec3& direction) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->DeepRaymarch(origin, direction);
}

template <uint32_t SS, uint8_t CS>
inline typename SparseOctree<Voxel, SS>::Hit ChunkManager<SS, CS>::DeepRaymarch(const glm::u8vec3& coordinate, typename SparseOctree<Voxel, SS>::Reader& session, const glm::vec3& origin, const glm::vec3& direction) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Data->DeepRaymarch(session, origin, direction);
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::FlushUpdates(const std::vector<uint32_t>& ids) {
  /// TODO: When flush is done, TSignal::Set(0, CHUNK_MANAGER_FLUSH_RENDER);
  /// TODO: Need to FlushUpdates() only chunks that are dirty

  for (uint8_t z = 0; z < CHUNK_SIZE; z++)
    for (uint8_t y = 0; y < CHUNK_SIZE; y++)
      for (uint8_t x = 0; x < CHUNK_SIZE; x++)
        if (m_Chunks->Exists(x, y, z))
          m_Chunks->Get(x, y, z)->Data->FlushUpdates({x, y, z}, ids);
}

template <uint32_t SS, uint8_t CS>
inline const std::vector<typename Chunk<SS>::FlushedChunk>& ChunkManager<SS, CS>::FlushRenderer(VkCommandBuffer commandbuffer) {
  /// TODO: Need to FlushRenderer() only chunks that are dirty

  m_FlushedChunks.clear();

  for (uint8_t z = 0; z < CHUNK_SIZE; z++)
    for (uint8_t y = 0; y < CHUNK_SIZE; y++)
      for (uint8_t x = 0; x < CHUNK_SIZE; x++)
        if (m_Chunks->Exists(x, y, z))
          m_FlushedChunks.emplace_back(m_Chunks->Get(x, y, z)->Data->FlushRenderer(commandbuffer));

  return m_FlushedChunks;
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
    return (a >= 0) ? (a / size) : ((a - size + 1) / size);
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

} // namespace vxen