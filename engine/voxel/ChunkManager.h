#pragma once

#include <cstdint>
#include <deque>
#include <glm/glm.hpp>

#include "Chunk.h"
#include "SparseOctree.h"
#include "Type.h"
#include "Voxel.h"

template <uint32_t SS, uint8_t CS>
class ChunkManager {
public:
  static constexpr uint32_t SVO_SIZE   = SS;
  static constexpr uint32_t CHUNK_SIZE = CS;

private:
  std::deque<Chunk<SS>>        m_ChunkAllocator{};
  SparseOctree<Chunk<SS>, CS>* m_Chunks = nullptr;

public:
  ChunkManager()
      : m_Chunks(new SparseOctree<Chunk<SS>, CS>()) {
    {
      auto write = m_Chunks->BeginWrite();

      for (size_t z = 0; z < CS; z++)
        for (size_t y = 0; y < CS; y++)
          for (size_t x = 0; x < CS; x++)
            m_Chunks->Set(write, x, y, z, &m_ChunkAllocator.emplace_back());
    }

    m_Chunks->Sync();
  };
  ~ChunkManager() { delete m_Chunks; };

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
   * Retrieve an RCU {Reader} session for the chunk at coordinate.
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   */
  SparseOctree<Voxel>::Reader BeginRead(const glm::u8vec3& coordinate);

  /**
   * Retrieve an RCU {Writer} session for the chunk at coordinate.
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   */
  SparseOctree<Voxel>::Writer BeginWrite(const glm::u8vec3& coordinate);

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
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   * @param data The voxel node to insert.
   */
  void Set(const glm::u8vec3& coordinate, SparseOctree<Voxel>::Writer& session, uint8_t x, uint8_t y, uint8_t z, Voxel* data);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Get(const glm::u8vec3& coordinate, uint8_t x, uint8_t y, uint8_t z);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param position The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Get(const glm::u8vec3& coordinate, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Get(const glm::u8vec3& coordinate, SparseOctree<Voxel>::Reader& session, uint8_t x, uint8_t y, uint8_t z);

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
  void Clear(const glm::u8vec3& coordinate, SparseOctree<Voxel>::Writer& session, const glm::u8vec3& position);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @return A vector containing the flattened octree for the chunk at coordinate.
   */
  const std::vector<SparseOctree<Voxel>::FlatNode>& Flatten(const glm::u8vec3& coordinate);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @return A vector containing the flattened octree for the chunk at coordinate.
   */
  const std::vector<SparseOctree<Voxel>::FlatNode>& Flatten(const glm::u8vec3& coordinate, SparseOctree<Voxel>::Reader& session);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param offset The chunk offset in world space. This will be used to apply the correct world space offset to each vertex. ((offset.x * 64) + localX)
   * @param ids The ids of all the voxels to greedy mesh. (SparseOctree requires that the .Data pointer contains an object that has .Id)
   * @return A vector containing the vertices of the octree for the chunk at coordinate.
   */
  const std::vector<Vertex>& GreedyMesh(const glm::u8vec3& coordinate, const glm::ivec3& offset, const std::vector<uint32_t>& ids);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param offset The chunk offset in world space. This will be used to apply the correct world space offset to each vertex. ((offset.x * 64) + localX)
   * @param ids The ids of all the voxels to greedy mesh. (SparseOctree requires that the .Data pointer contains an object that has .Id)
   * @return A vector containing the vertices of the octree for the chunk at coordinate.
   */
  const std::vector<Vertex>& GreedyMesh(const glm::u8vec3& coordinate, SparseOctree<Voxel>::Reader& session, const glm::ivec3& offset, const std::vector<uint32_t>& ids);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param out The vector in which all filtered nodes will be placed into. Make sure the vector is thread safe.
   * @param filter The filter callback, must return a boolean.
   */
  template <typename F>
    requires FilterCallback<SparseOctree<Voxel>::Node, F>
  void Filter(const glm::u8vec3& coordinate, std::vector<SparseOctree<Voxel>::FilterNode>& out, F&& filter);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param out The vector in which all filtered nodes will be placed into. Make sure the vector is thread safe.
   * @param filter The filter callback, must return a boolean.
   */
  template <typename F>
    requires FilterCallback<SparseOctree<Voxel>::Node, F>
  void Filter(const glm::u8vec3& coordinate, SparseOctree<Voxel>::Reader& session, std::vector<SparseOctree<Voxel>::FilterNode>& out, F&& filter);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param origin The camera's origin
   * @param direction The camera's direction
   * @return The voxel that was hit
   */
  SparseOctree<Voxel>::Hit DeepRaymarch(const glm::u8vec3& coordinate, const glm::vec3& origin, const glm::vec3& direction);

  /**
   * @param coordinate The coordinate of the node in the the Chunk SVO.
   * @param session The {Reader} session received from BeginRead(...)
   * @param origin The camera's origin
   * @param direction The camera's direction
   * @return The voxel that was hit
   */
  SparseOctree<Voxel>::Hit DeepRaymarch(const glm::u8vec3& coordinate, SparseOctree<Voxel>::Reader& session, const glm::vec3& origin, const glm::vec3& direction);

  /**
   * Converts world coordinates (96, 32, 32) to chunk coordinates (1, 0, 0)
   */
  static inline glm::u8vec3 WorldToChunkCoordinate(float x, float y, float z);

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
inline SparseOctree<Voxel>::Reader ChunkManager<SS, CS>::BeginRead(const glm::u8vec3& coordinate) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->BeginRead();
}

template <uint32_t SS, uint8_t CS>
inline SparseOctree<Voxel>::Writer ChunkManager<SS, CS>::BeginWrite(const glm::u8vec3& coordinate) {
  return m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->BeginWrite();
}

template <uint32_t SS, uint8_t CS>
inline void ChunkManager<SS, CS>::Sync(const glm::u8vec3& coordinate) {
  m_Chunks->Get(coordinate.x, coordinate.y, coordinate.z)->Sync();
}

template <uint32_t SS, uint8_t CS>
inline glm::u8vec3 ChunkManager<SS, CS>::WorldToChunkCoordinate(float x, float y, float z) {
  auto floor = [](float a, float b) {
    return (a >= 0) ? (a / b) : ((a - b + 1) / b);
  };
  return glm::u8vec3(floor(x, SS), floor(y, SS), floor(z, SS));
}

template <uint32_t SS, uint8_t CS>
inline uint8_t ChunkManager<SS, CS>::Wrap(int i) {
  if (i == -1)
    return SS - 1;
  if (i == SS)
    return 0;
  return i;
}