#pragma once

#include <bitset>
#include <cstring>
#include <vector>

#include <glm/glm.hpp>

#include "Face.h"
#include "SparseVoxelOctree.h"

#include <immintrin.h>

class GreedyMesh32 {
public:
  static constexpr uint8_t CHUNK_SIZE = 32;
  static constexpr unsigned int MASK_LENGTH = CHUNK_SIZE * CHUNK_SIZE;

private:
  static uint32_t ClearLowestBits(uint32_t bits, int n) {
    return (n >= CHUNK_SIZE) ? 0 : (bits & ~((1U << n) - 1));
  }

  static void SetWidthHeight(uint8_t a, uint8_t b, uint32_t bits,
                             uint32_t (&widthMasks)[],
                             uint32_t (&heightMasks)[]);

  static void
  PrepareWidthHeightMasks(const uint32_t (&bits)[], uint8_t paddingIndex,
                          uint8_t (&padding)[], uint32_t (&widthStart)[],
                          uint32_t (&heightStart)[], uint32_t (&widthEnd)[],
                          uint32_t (&heightEnd)[]);

  static void GreedyMeshFace(const glm::ivec3 &offsetPosition, uint8_t a,
                               uint8_t b, uint32_t bits,
                               uint32_t (&widthMasks)[],
                               uint32_t (&heightMasks)[],
                               std::vector<Vertex> &vertices, FaceType type,
                               uint32_t material);

  static void
  GreedyMeshAxis(const glm::ivec3 &offsetPosition, const uint32_t (&bits)[],
                   uint32_t (&widthStart)[], uint32_t (&heightStart)[],
                   uint32_t (&widthEnd)[], uint32_t (&heightEnd)[],
                   std::vector<Vertex> &vertices, FaceType startType,
                   FaceType endType, uint32_t material);

public:
  static void Octree(SparseVoxelOctree *tree, std::vector<Vertex> &vertices,
                     int originX, int originY, int originZ,
                     uint32_t material = 0);
};