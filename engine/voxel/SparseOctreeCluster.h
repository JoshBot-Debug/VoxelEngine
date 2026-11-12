#pragma once

#pragma once

#include "SparseOctree.h"

class Vertex;

class SparseOctreeCluster {
private:
  std::vector<std::unique_ptr<SparseOctree>> m_Trees {};
  std::unique_ptr<SparseOctree>              m_SCO {nullptr};
  std::vector<std::vector<Vertex>>           m_Vertices {};

private:
  /**
   * Enures that a tree will be returned, creates one if it does not exist.
   * @param x,y,z The position of the tree.
   */
  Node* Ensure(uint8_t x, uint8_t y, uint8_t z);

public:
  SparseOctreeCluster();
  ~SparseOctreeCluster() = default;

  SparseOctreeCluster(const SparseOctreeCluster&)            = delete;
  SparseOctreeCluster& operator=(const SparseOctreeCluster&) = delete;

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
   * Performs a regular greedy mesh on the specified coordinate.
   */
  const std::vector<Vertex>& GreedyMesh(uint8_t x, uint8_t y, uint8_t z);

  /**
   * @returns The cluster SVO tree. DO NOT STORE ANYWHERE, THIS IS A UNIQUE PTR
   */
  SparseOctree* SVO();

  /**
   * @returns The vector containing all chunk SVO trees
   */
  const std::vector<std::unique_ptr<SparseOctree>>& Clusters();
};