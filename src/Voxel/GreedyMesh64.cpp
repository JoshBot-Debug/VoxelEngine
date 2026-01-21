#include "GreedyMesh64.h"
#include <iostream>

void GreedyMesh64::SetWidthHeight(uint8_t a, uint8_t b, uint64_t bits,
                                  uint64_t (&widthMasks)[],
                                  uint64_t (&heightMasks)[]) {
  while (bits) {
    const uint8_t w = __builtin_ffsll(bits) - 1;

    const unsigned int wi = a + (CHUNK_SIZE * (w + (CHUNK_SIZE * b)));
    widthMasks[wi / CHUNK_SIZE] |= (1ULL << (wi % CHUNK_SIZE));

    const unsigned int hi = b + (CHUNK_SIZE * (w + (CHUNK_SIZE * a)));
    heightMasks[hi / CHUNK_SIZE] |= (1ULL << (hi % CHUNK_SIZE));

    bits = ClearLowestBits(bits, w + 1);
  }
}

void GreedyMesh64::PrepareWidthHeightMasks(
    const uint64_t (&bits)[], uint8_t paddingIndex, uint8_t (&padding)[],
    uint64_t (&widthStart)[], uint64_t (&heightStart)[], uint64_t (&widthEnd)[],
    uint64_t (&heightEnd)[]) {

  for (uint8_t a = 0; a < CHUNK_SIZE; a++)
    for (uint8_t b = 0; b < CHUNK_SIZE; b++) {

      unsigned int i = a + (CHUNK_SIZE * b);

      /**
       * Get the bitmask at index a,b
       * The padding mask has an extra bit as the LSB and MSB.
       * The MSB is the LSB of the pervious neighbour chunk
       * The LSB is the MSB of the next neighbour chunk
       * The first & last will always be a zero because there is no neighbour
       * next to them. 0...1 => 1...1 => 1...0
       */
      const uint8_t paddingMask = padding[i];

      /**
       * Shift right to remove the LSB padding bit and extract the following
       * 64bits into a new mask This is the actual mask we will use for the
       * height and width
       */
      const uint64_t mask = bits[i];

      /**
       * The first bit that is on on the right/top/front
       */
      const unsigned int msbIndex =
          (mask == 0) ? (CHUNK_SIZE - 1)
                      : (CHUNK_SIZE - 1) - __builtin_clzll(mask);

      /**
       * The first bit that is on on the left/bottom/back
       */
      const unsigned int lsbIndex = (mask == 0) ? 0 : __builtin_ctzll(mask);

      /**
       * Remove all the bits other than the start face
       * 11100111100011 => 00100000100001
       */
      uint64_t startMask = mask & ~(mask << 1);

      /**
       * Likewise remove all the bits other than the end face
       * 11100111100011 => 10000100000010
       */
      uint64_t endMask = mask & ~(mask >> 1);

      /**
       * Check the padding mask, if the bit at 0 index is on
       * turn off the MSB of the start mask
       *
       * If bit 0 of paddingMask is set, then clear bit 0 of startMask.
       * if ((paddingMask >> 0) & 1)
       *   startMask &= ~(1ULL << 0);
       */
      if ((paddingMask >> paddingIndex) & 1)
        startMask &= ~(1ULL << lsbIndex);

      /**
       * Check the padding mask, if the bit at 63 index is on
       * turn off the LSB of the end mask
       *
       * This is done in order to not set the height & width of the face at the
       * end of the chunk if the neighbour is the same to avoid creating faces
       * inbetween chunks
       *
       * If bit 63 of paddingMask is set, then clear bit 63 of endMask.
       * if ((paddingMask >> 63) & 1)
       *   endMask &= ~(1ULL << 63);
       */
      if ((paddingMask >> (paddingIndex + 1)) & 1)
        endMask &= ~(1ULL << msbIndex);

      SetWidthHeight(a, b, startMask, widthStart, heightStart);
      SetWidthHeight(a, b, endMask, widthEnd, heightEnd);
    }
}

void GreedyMesh64::GreedyMeshFace(const glm::ivec3 &offsetPosition, uint8_t a,
                                  uint8_t b, uint64_t bits,
                                  uint64_t (&widthMasks)[],
                                  uint64_t (&heightMasks)[],
                                  std::vector<Vertex> &vertices, FaceType type,
                                  uint32_t material) {
  while (bits) {
    const uint8_t w = __builtin_ffsll(bits) - 1;
    bits = ClearLowestBits(bits, w + 1);

    const uint64_t &width =
        ClearLowestBits(widthMasks[(w + (CHUNK_SIZE * a))], b);

    if (!width)
      continue;

    const uint8_t widthOffset = __builtin_ffsll(width) - 1;

    uint8_t widthSize =
        ~width == 0 ? CHUNK_SIZE : __builtin_ctzll(~(width >> widthOffset));

    const uint64_t &height =
        ClearLowestBits(heightMasks[w + (CHUNK_SIZE * (int)(widthOffset))], a);

    const uint8_t heightOffset = __builtin_ffsll(height) - 1;

    uint8_t heightSize =
        ~height == 0 ? CHUNK_SIZE : __builtin_ctzll(~(height >> heightOffset));

    for (uint8_t i = heightOffset; i < heightOffset + heightSize; i++) {
      const unsigned int index = w + (CHUNK_SIZE * i);

      const uint64_t widthSizeMask =
          (((widthSize >= CHUNK_SIZE ? 0ULL : (1ULL << widthSize)) - 1)
           << widthOffset);

      const uint64_t size = widthMasks[index] & widthSizeMask;

      if (size == 0 ||
          (size != ~0ULL && __builtin_ctzll(~(size >> (__builtin_ffsll(size) -
                                                       1))) != widthSize)) {
        heightSize = i - heightOffset;
        break;
      }

      widthMasks[index] &= ~widthSizeMask;
    }

    switch (type) {
    case FaceType::TOP:
      Face::Top(vertices, widthOffset + (offsetPosition.x * CHUNK_SIZE),
                w + (offsetPosition.y * CHUNK_SIZE),
                a + (offsetPosition.z * CHUNK_SIZE), widthSize, 1.0f,
                heightSize, material);
      break;
    case FaceType::BOTTOM:
      Face::Bottom(vertices, widthOffset + (offsetPosition.x * CHUNK_SIZE),
                   w + (offsetPosition.y * CHUNK_SIZE),
                   a + (offsetPosition.z * CHUNK_SIZE), widthSize, 1.0f,
                   heightSize, material);
      break;

    case FaceType::LEFT:
      Face::Left(vertices, w + (offsetPosition.x * CHUNK_SIZE),
                 widthOffset + (offsetPosition.y * CHUNK_SIZE),
                 a + (offsetPosition.z * CHUNK_SIZE), 1.0f, widthSize,
                 heightSize, material);
      break;
    case FaceType::RIGHT:
      Face::Right(vertices, w + (offsetPosition.x * CHUNK_SIZE),
                  widthOffset + (offsetPosition.y * CHUNK_SIZE),
                  a + (offsetPosition.z * CHUNK_SIZE), 1.0f, widthSize,
                  heightSize, material);

      break;
    case FaceType::FRONT:
      Face::Front(vertices, a + (offsetPosition.x * CHUNK_SIZE),
                  widthOffset + (offsetPosition.y * CHUNK_SIZE),
                  w + (offsetPosition.z * CHUNK_SIZE), heightSize, widthSize,
                  1.0f, material);
      break;
    case FaceType::BACK:
      Face::Back(vertices, a + (offsetPosition.x * CHUNK_SIZE),
                 widthOffset + (offsetPosition.y * CHUNK_SIZE),
                 w + (offsetPosition.z * CHUNK_SIZE), heightSize, widthSize,
                 1.0f, material);
      break;
    }
  }
}

void GreedyMesh64::GreedyMeshAxis(
    const glm::ivec3 &offsetPosition, const uint64_t (&bits)[],
    uint64_t (&widthStart)[], uint64_t (&heightStart)[], uint64_t (&widthEnd)[],
    uint64_t (&heightEnd)[], std::vector<Vertex> &vertices, FaceType startType,
    FaceType endType, uint32_t material) {
  for (uint8_t a = 0; a < CHUNK_SIZE; a++)
    for (uint8_t b = 0; b < CHUNK_SIZE; b++) {
      const uint64_t mask = bits[b + (CHUNK_SIZE * a)];

      GreedyMeshFace(offsetPosition, a, b, mask & ~(mask << 1), widthStart,
                     heightStart, vertices, startType, material);
      GreedyMeshFace(offsetPosition, a, b, mask & ~(mask >> 1), widthEnd,
                     heightEnd, vertices, endType, material);
    }
}

void GreedyMesh64::Octree(SparseVoxelOctree *tree,
                          std::vector<Vertex> &vertices, int chunkX, int chunkY,
                          int chunkZ, uint32_t material) {
  const glm::vec3 coord = {chunkX, chunkY, chunkZ};

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
  alignas(32) uint64_t rows[MASK_LENGTH] = {};
  alignas(32) uint64_t columns[MASK_LENGTH] = {};
  alignas(32) uint64_t layers[MASK_LENGTH] = {};
  alignas(32) uint8_t padding[MASK_LENGTH] = {};

  bool hasVoxels = false;

  for (int x = 0; x < CHUNK_SIZE; x++)
    for (int y = 0; y < CHUNK_SIZE; y++)
      for (int z = 0; z < CHUNK_SIZE; z++)
        if (tree->Get(x + origin.x, y + origin.y, z + origin.z, material)) {
          hasVoxels = true;

          const unsigned int rowIndex =
              x + (CHUNK_SIZE * (y + (CHUNK_SIZE * z)));
          const unsigned int columnIndex =
              y + (CHUNK_SIZE * (x + (CHUNK_SIZE * z)));
          const unsigned int layerIndex =
              z + (CHUNK_SIZE * (y + (CHUNK_SIZE * x)));

          rows[rowIndex / CHUNK_SIZE] |= (1ULL << (rowIndex % CHUNK_SIZE));
          columns[columnIndex / CHUNK_SIZE] |=
              (1ULL << (columnIndex % CHUNK_SIZE));
          layers[layerIndex / CHUNK_SIZE] |=
              (1ULL << (layerIndex % CHUNK_SIZE));
        }

  if (!hasVoxels)
    return;

  /**
   * Cull meshing, ~0.13ms slower than greedy meshing
   *
   * CullMesh(coord, vertices, columns, rows, layers, CHUNK_SIZE);
   * return;
   */

  /**
   * Here we capture the padding bit
   * Every chunk we need to get the neighbour chunks and check
   * if the bits at the 0 & 31st index are on. If so, we need to skip making
   * faces on that end.
   */
  for (int i = 0; i < MASK_LENGTH; i++) {
    uint64_t &row = rows[i];
    uint64_t &column = columns[i];
    uint64_t &layer = layers[i];

    int fast = i % CHUNK_SIZE;
    int slow = (i / CHUNK_SIZE) % CHUNK_SIZE;
    int MSB = 0;
    int LSB = 0;

    if (row > 0) {
      MSB = CHUNK_SIZE - __builtin_clzll(row);
      LSB = __builtin_ctzll(row) - 1;

      if (tree->Get(origin.x + MSB, fast + origin.y, slow + origin.z))
        padding[i] |= (1ULL << 1);

      if (tree->Get(origin.x + LSB, fast + origin.y, slow + origin.z))
        padding[i] |= (1ULL << 0);
    }

    if (column > 0) {
      MSB = CHUNK_SIZE - __builtin_clzll(column);
      LSB = __builtin_ctzll(column) - 1;

      if (tree->Get(fast + origin.x, origin.y + MSB, slow + origin.z))
        padding[i] |= (1ULL << 3);

      if (tree->Get(fast + origin.x, origin.y + LSB, slow + origin.z))
        padding[i] |= (1ULL << 2);
    }

    if (layer > 0) {
      MSB = CHUNK_SIZE - __builtin_clzll(layer);
      LSB = __builtin_ctzll(layer) - 1;

      if (tree->Get(slow + origin.x, fast + origin.y, origin.z + MSB))
        padding[i] |= (1ULL << 5);

      if (tree->Get(slow + origin.x, fast + origin.y, origin.z + LSB))
        padding[i] |= (1ULL << 4);
    }
  }

  alignas(32) uint64_t widthStart[MASK_LENGTH] = {};
  alignas(32) uint64_t heightStart[MASK_LENGTH] = {};

  alignas(32) uint64_t widthEnd[MASK_LENGTH] = {};
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

  PrepareWidthHeightMasks(rows, 0, padding, widthStart, heightStart, widthEnd,
                          heightEnd);

  GreedyMeshAxis(coord, rows, widthStart, heightStart, widthEnd, heightEnd,
                 vertices, FaceType::LEFT, FaceType::RIGHT, material);

  std::memset(widthStart, 0, sizeof(widthStart));
  std::memset(heightStart, 0, sizeof(heightStart));
  std::memset(widthEnd, 0, sizeof(widthEnd));
  std::memset(heightEnd, 0, sizeof(heightEnd));

  PrepareWidthHeightMasks(columns, 2, padding, widthStart, heightStart,
                          widthEnd, heightEnd);

  GreedyMeshAxis(coord, columns, widthStart, heightStart, widthEnd, heightEnd,
                 vertices, FaceType::BOTTOM, FaceType::TOP, material);

  std::memset(widthStart, 0, sizeof(widthStart));
  std::memset(heightStart, 0, sizeof(heightStart));
  std::memset(widthEnd, 0, sizeof(widthEnd));
  std::memset(heightEnd, 0, sizeof(heightEnd));

  PrepareWidthHeightMasks(layers, 4, padding, widthStart, heightStart, widthEnd,
                          heightEnd);
  GreedyMeshAxis(coord, layers, widthStart, heightStart, widthEnd, heightEnd,
                 vertices, FaceType::FRONT, FaceType::BACK, material);
}