#pragma once

#include "Face.h"
#include "SparseVoxelOctree.h"

#include <cstring>
#include <glm/glm.hpp>
#include <immintrin.h>
#include <vector>

class GreedyMesh64 {
public:
  static constexpr uint8_t      CHUNK_SIZE  = 64;
  static constexpr unsigned int MASK_LENGTH = CHUNK_SIZE * CHUNK_SIZE;

private:
  static uint64_t ClearLowestBits(uint64_t bits, int n) {
    return (n >= CHUNK_SIZE) ? 0 : (bits & ~((1ULL << n) - 1));
  }

  static void SetWidthHeight(uint8_t a, uint8_t b, uint64_t bits,
                             uint64_t (&widthMasks)[],
                             uint64_t (&heightMasks)[]);

  static void
  PrepareWidthHeightMasks(const uint64_t (&bits)[], uint8_t paddingIndex,
                          uint8_t (&padding)[], uint64_t (&widthStart)[],
                          uint64_t (&heightStart)[], uint64_t (&widthEnd)[],
                          uint64_t (&heightEnd)[]);

  static void GreedyMeshFace(const glm::ivec3& offsetPosition, uint8_t a,
                             uint8_t b, uint64_t bits, uint64_t (&widthMasks)[],
                             uint64_t (&heightMasks)[],
                             std::vector<Vertex>& vertices, FaceType type,
                             uint32_t material);

  static void GreedyMeshAxis(const glm::ivec3& offsetPosition,
                             const uint64_t (&bits)[], uint64_t (&widthStart)[],
                             uint64_t (&heightStart)[], uint64_t (&widthEnd)[],
                             uint64_t (&heightEnd)[],
                             std::vector<Vertex>& vertices, FaceType startType,
                             FaceType endType, uint32_t material);

public:
  static void Octree(SparseVoxelOctree* tree, std::vector<Vertex>& vertices,
                     int originX, int originY, int originZ,
                     uint32_t material = 0);
};