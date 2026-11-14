#pragma once

#include <vk_mem_alloc.h>

#include "render/Buffer.h"
#include "render/BufferPool.h"
#include "voxel/SparseOctreeCluster.h"

#include "Chunk.h"

namespace vxen {

using namespace akari::render;

class ChunkManager {
public:
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

  struct Buffers {
    VkBuffer SVO       = VK_NULL_HANDLE;
    VkBuffer SVOHeader = VK_NULL_HANDLE;
    VkBuffer Vertex    = VK_NULL_HANDLE;
    VkBuffer ChunkSVO  = VK_NULL_HANDLE;
    VkBuffer Chunk     = VK_NULL_HANDLE;

    VkBufferMemoryBarrier2 SVOBarrier;
    VkBufferMemoryBarrier2 SVOHeaderBarrier;
    VkBufferMemoryBarrier2 VertexBarrier;
    VkBufferMemoryBarrier2 ChunkSVOBarrier;
    VkBufferMemoryBarrier2 ChunkBarrier;
  };

private:
  SparseOctreeCluster m_SCO;

  BufferPool m_SVOPool {};
  BufferPool m_SVOHeaderPool {};
  BufferPool m_VertexPool {};

  Buffer m_SVOBuffer {};
  Buffer m_SVOHeaderBuffer {};
  Buffer m_VertexBuffer {};
  Buffer m_ChunkSVOBuffer {};
  Buffer m_ChunkBuffer {};

public:
  ChunkManager();
  ~ChunkManager() = default;

  ChunkManager(const ChunkManager&)            = delete;
  ChunkManager& operator=(const ChunkManager&) = delete;

  /**
   * Get all buffers
   */
  Buffers GetBuffers();

  /**
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   * @param id The voxel node to insert.
   */
  void Set(uint32_t x, uint32_t y, uint32_t z, uint32_t id);

  /**
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  Node* Get(uint32_t x, uint32_t y, uint32_t z);

  /**
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  bool Exists(uint32_t x, uint32_t y, uint32_t z);

  /**
   * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
   */
  void Clear(uint32_t x, uint32_t y, uint32_t z);

  /**
   * @param origin The camera's origin
   * @param direction The camera's direction
   * @return The voxel that was hit
   */
  SparseOctree::Hit Raymarch(const glm::vec3& origin, const glm::vec3& direction);

  /**
   * Collects all flushed chunks that are dirty
   * @param commandBuffer The command buffer for this frame
   */
  void FlushPreprocessor(VkCommandBuffer commandBuffer);
};
} // namespace vxen
