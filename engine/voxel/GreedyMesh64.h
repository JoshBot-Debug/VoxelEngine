#pragma once

#include "Face.h"

#include <cstring>
#include <glm/glm.hpp>
#include <immintrin.h>
#include <vector>

template <typename F>
concept ExistsCallback =
    requires(F f, int x, int y, int z) {
      { f(x, y, z) } -> std::same_as<bool>;
    };

class GreedyMesh64 {
public:
  static constexpr uint8_t      CHUNK_SIZE  = 64;
  static constexpr unsigned int MASK_LENGTH = CHUNK_SIZE * CHUNK_SIZE;

private:
  static inline uint64_t ClearLowestBits(uint64_t bits, int n) {
    return (n == 64) ? 0 : (bits & (~0ULL << n));
  }

  static inline void SetWidthHeight(uint8_t a, uint8_t b, uint64_t bits, uint64_t (&widthMasks)[MASK_LENGTH], uint64_t (&heightMasks)[MASK_LENGTH]);

  static void PrepareWidthHeightMasks(const uint64_t (&bits)[MASK_LENGTH], uint8_t paddingIndex, uint8_t (&padding)[MASK_LENGTH], uint64_t (&widthStart)[MASK_LENGTH], uint64_t (&heightStart)[MASK_LENGTH], uint64_t (&widthEnd)[MASK_LENGTH], uint64_t (&heightEnd)[MASK_LENGTH]);

  static inline void GreedyMeshFace(const glm::ivec3& offset, uint8_t a, uint8_t b, uint64_t bits, uint64_t (&widthMasks)[MASK_LENGTH], uint64_t (&heightMasks)[MASK_LENGTH], std::vector<Vertex>& vertices, FaceType type, uint32_t id);

  static void GreedyMeshAxis(const glm::ivec3& offset,
                             const uint64_t (&bits)[MASK_LENGTH],
                             uint64_t (&widthStart)[MASK_LENGTH],
                             uint64_t (&heightStart)[MASK_LENGTH],
                             uint64_t (&widthEnd)[MASK_LENGTH],
                             uint64_t (&heightEnd)[MASK_LENGTH],
                             std::vector<Vertex>& vertices,
                             FaceType             startType,
                             FaceType             endType,
                             uint32_t             id);

public:
  template <ExistsCallback F>
  static void GeneratePadding(uint8_t (&padding)[MASK_LENGTH],
                              uint64_t (&rows)[MASK_LENGTH],
                              uint64_t (&columns)[MASK_LENGTH],
                              uint64_t (&layers)[MASK_LENGTH],
                              F&& exists) {

    for (int i = 0; i < MASK_LENGTH; i++) {
      uint64_t& row    = rows[i];
      uint64_t& column = columns[i];
      uint64_t& layer  = layers[i];

      int fast = i & (CHUNK_SIZE - 1);
      int slow = (i >> 6) & (CHUNK_SIZE - 1);
      int MSB  = 0;
      int LSB  = 0;

      if (row > 0) {
        MSB = CHUNK_SIZE - __builtin_clzll(row);
        LSB = __builtin_ctzll(row) - 1;

        if (exists(MSB, fast, slow))
          padding[i] |= (1ULL << 1);

        if (exists(LSB, fast, slow))
          padding[i] |= (1ULL << 0);
      }

      if (column > 0) {
        MSB = CHUNK_SIZE - __builtin_clzll(column);
        LSB = __builtin_ctzll(column) - 1;

        if (exists(fast, MSB, slow))
          padding[i] |= (1ULL << 3);

        if (exists(fast, LSB, slow))
          padding[i] |= (1ULL << 2);
      }

      if (layer > 0) {
        MSB = CHUNK_SIZE - __builtin_clzll(layer);
        LSB = __builtin_ctzll(layer) - 1;

        if (exists(slow, fast, MSB))
          padding[i] |= (1ULL << 5);

        if (exists(slow, fast, LSB))
          padding[i] |= (1ULL << 4);
      }
    }
  }

  /**
   * Culls the column/row/layer
   * From: 001110110011111
   * To: 001010110010001
   *
   * Loop over the correct axis for each column/row/layer
   * for columns x,z
   * for rows    y,z
   * for layers  y,x
   *
   * Set the width and height of each face for both sides (start & end)
   * for columns bottom & top
   * for rows    left & right
   * for layers  front & back
   *
   * Then greedy meshes rows/columns/layers
   */
  static void Generate(
      std::vector<Vertex>& vertices,
      const glm::vec3&     coord,
      uint32_t             id,
      uint64_t (&rows)[MASK_LENGTH],
      uint64_t (&columns)[MASK_LENGTH],
      uint64_t (&layers)[MASK_LENGTH],
      uint8_t (&padding)[MASK_LENGTH]) {

    uint64_t widthStart[MASK_LENGTH]  = {};
    uint64_t heightStart[MASK_LENGTH] = {};
    uint64_t widthEnd[MASK_LENGTH]    = {};
    uint64_t heightEnd[MASK_LENGTH]   = {};

    PrepareWidthHeightMasks(rows, 0, padding, widthStart, heightStart, widthEnd, heightEnd);
    GreedyMeshAxis(coord, rows, widthStart, heightStart, widthEnd, heightEnd, vertices, FaceType::LEFT, FaceType::RIGHT, id);

    PrepareWidthHeightMasks(columns, 2, padding, widthStart, heightStart, widthEnd, heightEnd);
    GreedyMeshAxis(coord, columns, widthStart, heightStart, widthEnd, heightEnd, vertices, FaceType::BOTTOM, FaceType::TOP, id);

    PrepareWidthHeightMasks(layers, 4, padding, widthStart, heightStart, widthEnd, heightEnd);
    GreedyMeshAxis(coord, layers, widthStart, heightStart, widthEnd, heightEnd, vertices, FaceType::FRONT, FaceType::BACK, id);
  };
};