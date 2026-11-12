// #pragma once

// #include <deque>
// #include <vector>
// #include <vk_mem_alloc.h>

// #include "render/Buffer.h"
// #include "render/BufferPool.h"
// #include "voxel/SparseOctree.h"

// #include "Chunk.h"

// namespace vxen {

// using namespace akari::render;

// class ChunkManager {
// public:
//   static constexpr uint32_t SVO_SIZE {64};
//   static constexpr uint32_t CHUNK_SIZE {64};

//   struct ChunkState {
//     uint32_t Empty {1};
//     uint32_t Offset[6] {0};
//     uint32_t Size[6] {0};
//   };

//   struct FlatNodeHeader {
//     uint32_t Id;
//     uint32_t Offset;
//     uint32_t Size;
//   };

//   struct Buffers {
//     VkBuffer SVO       = VK_NULL_HANDLE;
//     VkBuffer SVOHeader = VK_NULL_HANDLE;
//     VkBuffer Vertex    = VK_NULL_HANDLE;
//     VkBuffer ChunkSVO  = VK_NULL_HANDLE;
//     VkBuffer Chunk     = VK_NULL_HANDLE;

//     VkBufferMemoryBarrier2 SVOBarrier;
//     VkBufferMemoryBarrier2 SVOHeaderBarrier;
//     VkBufferMemoryBarrier2 VertexBarrier;
//     VkBufferMemoryBarrier2 ChunkSVOBarrier;
//     VkBufferMemoryBarrier2 ChunkBarrier;
//   };

// private:
//   static constexpr uint32_t DIV_SS {std::countr_zero(64U)};
//   static constexpr uint32_t MOD_SS {64U - 1};

//   BufferPool m_SVOPool {};
//   BufferPool m_SVOHeaderPool {};
//   BufferPool m_VertexPool {};

//   /**
//    * Keeps the Chunk allocations, [index + 1] is used as the ID for SCO
//    */
//   std::deque<Chunk> m_ChunkAllocations {};

//   /**
//    * SCO
//    * Sparse Chunk Octree
//    */
//   SparseOctree*                       m_SCO {nullptr};
//   std::vector<SparseOctree::FlatNode> m_SCONodes {};
//   std::vector<ChunkState>             m_ChunkState {};

//   Buffer m_SVOBuffer {};
//   Buffer m_SVOHeaderBuffer {};
//   Buffer m_VertexBuffer {};

//   Buffer m_ChunkSVOBuffer {};
//   Buffer m_ChunkBuffer {};

// private:
//   /**
//    * Enures that a chunk will be returned, creates one if it does not exist.
//    * @param x,y,z The position of the chunk.
//    */
//   Node* Ensure(uint32_t x, uint32_t y, uint32_t z);

// public:
//   ChunkManager();
//   ~ChunkManager();

//   ChunkManager(const ChunkManager&)            = delete;
//   ChunkManager& operator=(const ChunkManager&) = delete;

//   /**
//    * Flatten the Chunks in to a vector for the GPU
//    */
//   const std::vector<SparseOctree::FlatNode>& Flatten();

//   /**
//    * Get all buffers
//    */
//   Buffers GetBuffers();

//   /**
//    * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
//    * @param id The voxel node to insert.
//    */
//   void Set(uint32_t x, uint32_t y, uint32_t z, uint32_t id);

//   /**
//    * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
//    */
//   Node* Get(uint32_t x, uint32_t y, uint32_t z);

//   /**
//    * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
//    */
//   bool Exists(uint32_t x, uint32_t y, uint32_t z);

//   /**
//    * @param x,y,z The position of the voxel in the chunk. i.e. 0 - 63.
//    */
//   void Clear(uint32_t x, uint32_t y, uint32_t z);

//   /**
//    * Filters across all chunks
//    * @param out The vector in which all filtered nodes will be placed into. Make sure the vector is thread safe.
//    * @param filter The filter callback, must return a boolean.
//    */
//   template <typename F>
//     requires FilterCallback<Node, F>
//   void Filter(std::vector<SparseOctree::FilterNode>& out, F&& filter);

//   /**
//    * @param origin The camera's origin
//    * @param direction The camera's direction
//    * @return The voxel that was hit
//    */
//   SparseOctree::Hit Raymarch(const glm::vec3& origin, const glm::vec3& direction);

//   /**
//    * Triggers multi thread greedy meshing.
//    * @param ids Used to split greedy meshing, each id is meshed in a seperate thread. Generally these ids would represent different voxel types, each type can be merged together.
//    */
//   void FlushVertices(const std::vector<uint32_t>& ids);

//   /**
//    * Collects all flushed chunks that are dirty
//    * @param commandBuffer The command buffer for this frame
//    * @returns A vector of flushed chunks that need to be rendered. Uses to create indirect draw commands.
//    */
//   void FlushPreprocessor(VkCommandBuffer commandBuffer);

//   /**
//    * Move the origin to the boundary of the next chunk
//    */
//   static inline float TNextBoundary(const glm::vec3& localOrigin, const glm::vec3& direction);
// };

// template <typename F>
//   requires FilterCallback<Node, F>
// inline void ChunkManager::Filter(std::vector<SparseOctree::FilterNode>& out, F&& filter) {
//   m_SCO->ForEach([&out, &filter, &chunkAllocator = m_ChunkAllocations](const Node* node) {
//     std::vector<SparseOctree::FilterNode> tmp;
//     uint32_t                              chunkIndex = node->Id - 1;
//     Chunk&                                chunk      = chunkAllocator[chunkIndex];
//     chunk.SVO()->Filter(tmp, filter);
//     for (auto& n : tmp) {
//       auto offset = chunk.GetCoordinate();
//       n.Position.x += offset.x * CHUNK_SIZE;
//       n.Position.y += offset.y * CHUNK_SIZE;
//       n.Position.z += offset.z * CHUNK_SIZE;
//     }
//     out.insert(out.end(), std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
//   });
// }

// } // namespace vxen

#pragma once

#include <deque>
#include <vector>
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
  SparseOctreeCluster                 m_SCO;
  std::vector<SparseOctree::FlatNode> m_SCONodes {};
  std::vector<ChunkState>             m_ChunkState {};
  std::vector<std::vector<Vertex>>    m_ChunkVertices {};

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
};
} // namespace vxen
