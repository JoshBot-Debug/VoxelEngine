#include "GreedyMesh64.h"

void GreedyMesh64::SetWidthHeight(uint8_t a, uint8_t b, uint64_t bits, uint64_t (&widthMasks)[], uint64_t (&heightMasks)[]) {
  while (bits) {
    const uint8_t w = __builtin_ffsll(bits) - 1;

    const unsigned int wi = a + (CHUNK_SIZE * (w + (CHUNK_SIZE * b)));
    widthMasks[wi >> 6] |= (1ULL << (wi & 63));

    const unsigned int hi = b + (CHUNK_SIZE * (w + (CHUNK_SIZE * a)));
    heightMasks[hi >> 6] |= (1ULL << (hi & 63));

    bits = ClearLowestBits(bits, w + 1);
  }
}

void GreedyMesh64::PrepareWidthHeightMasks(
    const uint64_t (&bits)[],
    uint8_t paddingIndex,
    uint8_t (&padding)[],
    uint64_t (&widthStart)[],
    uint64_t (&heightStart)[],
    uint64_t (&widthEnd)[],
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
          (mask == 0) ? (CHUNK_SIZE - 1) : (CHUNK_SIZE - 1) - __builtin_clzll(mask);

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

void GreedyMesh64::GreedyMeshFace(const glm::ivec3& offsetPosition, uint8_t a, uint8_t b, uint64_t bits, uint64_t (&widthMasks)[], uint64_t (&heightMasks)[], std::vector<Vertex>& vertices, FaceType type, uint32_t id) {
  while (bits) {
    const uint8_t w = __builtin_ffsll(bits) - 1;
    bits            = ClearLowestBits(bits, w + 1);

    const uint64_t& width =
        ClearLowestBits(widthMasks[(w + (CHUNK_SIZE * a))], b);

    if (!width)
      continue;

    const uint8_t widthOffset = __builtin_ffsll(width) - 1;

    uint8_t widthSize =
        ~width == 0 ? CHUNK_SIZE : __builtin_ctzll(~(width >> widthOffset));

    const uint64_t& height =
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
      Face::Top(vertices, widthOffset + (offsetPosition.x * CHUNK_SIZE), w + (offsetPosition.y * CHUNK_SIZE), a + (offsetPosition.z * CHUNK_SIZE), widthSize, 1.0f, heightSize, id);
      break;
    case FaceType::BOTTOM:
      Face::Bottom(vertices, widthOffset + (offsetPosition.x * CHUNK_SIZE), w + (offsetPosition.y * CHUNK_SIZE), a + (offsetPosition.z * CHUNK_SIZE), widthSize, 1.0f, heightSize, id);
      break;

    case FaceType::LEFT:
      Face::Left(vertices, w + (offsetPosition.x * CHUNK_SIZE), widthOffset + (offsetPosition.y * CHUNK_SIZE), a + (offsetPosition.z * CHUNK_SIZE), 1.0f, widthSize, heightSize, id);
      break;
    case FaceType::RIGHT:
      Face::Right(vertices, w + (offsetPosition.x * CHUNK_SIZE), widthOffset + (offsetPosition.y * CHUNK_SIZE), a + (offsetPosition.z * CHUNK_SIZE), 1.0f, widthSize, heightSize, id);

      break;
    case FaceType::FRONT:
      Face::Front(vertices, a + (offsetPosition.x * CHUNK_SIZE), widthOffset + (offsetPosition.y * CHUNK_SIZE), w + (offsetPosition.z * CHUNK_SIZE), heightSize, widthSize, 1.0f, id);
      break;
    case FaceType::BACK:
      Face::Back(vertices, a + (offsetPosition.x * CHUNK_SIZE), widthOffset + (offsetPosition.y * CHUNK_SIZE), w + (offsetPosition.z * CHUNK_SIZE), heightSize, widthSize, 1.0f, id);
      break;
    }
  }
}

void GreedyMesh64::GreedyMeshAxis(
    const glm::ivec3& offsetPosition,
    const uint64_t (&bits)[],
    uint64_t (&widthStart)[],
    uint64_t (&heightStart)[],
    uint64_t (&widthEnd)[],
    uint64_t (&heightEnd)[],
    std::vector<Vertex>& vertices,
    FaceType             startType,
    FaceType             endType,
    uint32_t             id) {
  for (uint8_t a = 0; a < CHUNK_SIZE; a++)
    for (uint8_t b = 0; b < CHUNK_SIZE; b++) {
      const uint64_t mask = bits[b + (CHUNK_SIZE * a)];

      GreedyMeshFace(offsetPosition, a, b, mask & ~(mask << 1), widthStart, heightStart, vertices, startType, id);
      GreedyMeshFace(offsetPosition, a, b, mask & ~(mask >> 1), widthEnd, heightEnd, vertices, endType, id);
    }
}