#pragma once

#include "Face.h"

#include <cstring>
#include <glm/glm.hpp>
#include <immintrin.h>
#include <vector>

template <typename F>
concept ExistsCallback =
    requires(F f, float x, float y, float z, uint32_t id) {
      { f(x, y, z) } -> std::same_as<bool>;
      { f(x, y, z, id) } -> std::same_as<bool>;
    };

class GreedyMesh64 {
public:
  static constexpr uint8_t      CHUNK_SIZE  = 64;
  static constexpr unsigned int MASK_LENGTH = CHUNK_SIZE * CHUNK_SIZE;

private:
  static uint64_t ClearLowestBits(uint64_t bits, int n) {
    return (n >= CHUNK_SIZE) ? 0 : (bits & ~((1ULL << n) - 1));
  }

  static void SetWidthHeight(uint8_t a, uint8_t b, uint64_t bits, uint64_t (&widthMasks)[], uint64_t (&heightMasks)[]);

  static void
  PrepareWidthHeightMasks(const uint64_t (&bits)[], uint8_t paddingIndex, uint8_t (&padding)[], uint64_t (&widthStart)[], uint64_t (&heightStart)[], uint64_t (&widthEnd)[], uint64_t (&heightEnd)[]);

  static void GreedyMeshFace(const glm::ivec3& offsetPosition, uint8_t a, uint8_t b, uint64_t bits, uint64_t (&widthMasks)[], uint64_t (&heightMasks)[], std::vector<Vertex>& vertices, FaceType type, uint32_t id);

  static void GreedyMeshAxis(const glm::ivec3& offsetPosition,
                             const uint64_t (&bits)[],
                             uint64_t (&widthStart)[],
                             uint64_t (&heightStart)[],
                             uint64_t (&widthEnd)[],
                             uint64_t (&heightEnd)[],
                             std::vector<Vertex>& vertices,
                             FaceType             startType,
                             FaceType             endType,
                             uint32_t             id);

public:
  template <ExistsCallback F>
  static void Generate(F&& exists, std::vector<Vertex>& vertices, int originX, int originY, int originZ, uint32_t id) {
    const glm::vec3 coord = {originX, originY, originZ};

    const glm::vec3 origin = {
        coord.x * CHUNK_SIZE,
        coord.y * CHUNK_SIZE,
        coord.z * CHUNK_SIZE,
    };

    /**
     * Generate the bit mask for rows, columns and layers.
     *
     * For columns:
     *
     *       At (x, z) you can get the height of the column in one bitwise
     * operation and you can get the location of all the top & bottom faces in one
     * bitwise operation y0 y1 y2 y3 z0 x0 0  0  0  0 z0 x1 0  0  0  0 z0 x2 0  0
     * 0  0 z0 x3 0  0  0  0 z1 x0 0  0  0  0 z1 x1 0  0  0  0 z1 x2 0  0  0  0 z1
     * x3 0  0  0  0
     */
    alignas(32) uint64_t rows[MASK_LENGTH]    = {};
    alignas(32) uint64_t columns[MASK_LENGTH] = {};
    alignas(32) uint64_t layers[MASK_LENGTH]  = {};
    alignas(32) uint8_t  padding[MASK_LENGTH] = {};

    bool hasVoxels = false;

    for (int x = 0; x < CHUNK_SIZE; x++)
      for (int y = 0; y < CHUNK_SIZE; y++)
        for (int z = 0; z < CHUNK_SIZE; z++)
          if (exists(x + origin.x, y + origin.y, z + origin.z, id)) {
            hasVoxels = true;

            const unsigned int rowIndex    = x + (CHUNK_SIZE * (y + (CHUNK_SIZE * z)));
            const unsigned int columnIndex = y + (CHUNK_SIZE * (x + (CHUNK_SIZE * z)));
            const unsigned int layerIndex  = z + (CHUNK_SIZE * (y + (CHUNK_SIZE * x)));

            rows[rowIndex / CHUNK_SIZE] |= (1ULL << (rowIndex % CHUNK_SIZE));
            columns[columnIndex / CHUNK_SIZE] |= (1ULL << (columnIndex % CHUNK_SIZE));
            layers[layerIndex / CHUNK_SIZE] |= (1ULL << (layerIndex % CHUNK_SIZE));
          }

    if (!hasVoxels)
      return;

    /**
     * Here we capture the padding bit
     * Every chunk we need to get the neighbour chunks and check
     * if the bits at the 0 & 31st index are on. If so, we need to skip making
     * faces on that end.
     */
    for (int i = 0; i < MASK_LENGTH; i++) {
      uint64_t& row    = rows[i];
      uint64_t& column = columns[i];
      uint64_t& layer  = layers[i];

      int fast = i % CHUNK_SIZE;
      int slow = (i / CHUNK_SIZE) % CHUNK_SIZE;
      int MSB  = 0;
      int LSB  = 0;

      if (row > 0) {
        MSB = CHUNK_SIZE - __builtin_clzll(row);
        LSB = __builtin_ctzll(row) - 1;

        if (exists(origin.x + MSB, fast + origin.y, slow + origin.z))
          padding[i] |= (1ULL << 1);

        if (exists(origin.x + LSB, fast + origin.y, slow + origin.z))
          padding[i] |= (1ULL << 0);
      }

      if (column > 0) {
        MSB = CHUNK_SIZE - __builtin_clzll(column);
        LSB = __builtin_ctzll(column) - 1;

        if (exists(fast + origin.x, origin.y + MSB, slow + origin.z))
          padding[i] |= (1ULL << 3);

        if (exists(fast + origin.x, origin.y + LSB, slow + origin.z))
          padding[i] |= (1ULL << 2);
      }

      if (layer > 0) {
        MSB = CHUNK_SIZE - __builtin_clzll(layer);
        LSB = __builtin_ctzll(layer) - 1;

        if (exists(slow + origin.x, fast + origin.y, origin.z + MSB))
          padding[i] |= (1ULL << 5);

        if (exists(slow + origin.x, fast + origin.y, origin.z + LSB))
          padding[i] |= (1ULL << 4);
      }
    }

    alignas(32) uint64_t widthStart[MASK_LENGTH]  = {};
    alignas(32) uint64_t heightStart[MASK_LENGTH] = {};

    alignas(32) uint64_t widthEnd[MASK_LENGTH]  = {};
    alignas(32) uint64_t heightEnd[MASK_LENGTH] = {};

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
     */

    PrepareWidthHeightMasks(rows, 0, padding, widthStart, heightStart, widthEnd, heightEnd);
    GreedyMeshAxis(coord, rows, widthStart, heightStart, widthEnd, heightEnd, vertices, FaceType::LEFT, FaceType::RIGHT, id);

    std::memset(widthStart, 0, sizeof(widthStart));
    std::memset(heightStart, 0, sizeof(heightStart));
    std::memset(widthEnd, 0, sizeof(widthEnd));
    std::memset(heightEnd, 0, sizeof(heightEnd));

    PrepareWidthHeightMasks(columns, 2, padding, widthStart, heightStart, widthEnd, heightEnd);
    GreedyMeshAxis(coord, columns, widthStart, heightStart, widthEnd, heightEnd, vertices, FaceType::BOTTOM, FaceType::TOP, id);

    std::memset(widthStart, 0, sizeof(widthStart));
    std::memset(heightStart, 0, sizeof(heightStart));
    std::memset(widthEnd, 0, sizeof(widthEnd));
    std::memset(heightEnd, 0, sizeof(heightEnd));

    PrepareWidthHeightMasks(layers, 4, padding, widthStart, heightStart, widthEnd, heightEnd);
    GreedyMeshAxis(coord, layers, widthStart, heightStart, widthEnd, heightEnd, vertices, FaceType::FRONT, FaceType::BACK, id);
  };
};