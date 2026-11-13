#include "Allocator.h"

namespace akari::memory {

Allocation Allocator::Allocate(uint32_t id, uint32_t bytes) {
  // #ifdef DEBUG
  //   std::cout << "\nBuffer::Blocks::Allocate(" << id << ", " << bytes << ")" << std::endl;
  // #endif

  auto mi = m_Id.size();

  for (size_t i = 0; i < m_Id.size(); i++)
    if (m_Id[i] == id) {
      mi = i;
      break;
    }

  auto insertBlock = [&](uint32_t i, uint32_t id, uint32_t bytes, bool resized = false) {
    // Add a free block with the remaining bytes at position i + 1
    // If there are any left over bytes
    if (m_Size[i] - bytes) {
      m_Id.insert(m_Id.begin() + i + 1, UINT32_MAX);
      m_Size.insert(m_Size.begin() + i + 1, m_Size[i] - bytes);
      m_Offset.insert(m_Offset.begin() + i + 1, m_Offset[i] + bytes);
      m_Free.insert(m_Free.begin() + i + 1, true);
    }

    // Update the block at i
    m_Id[i]   = id;
    m_Size[i] = bytes;
    m_Free[i] = false;

    return Allocation {
        .Id      = id,
        .Size    = bytes,
        .Offset  = m_Offset[i],
        .Resized = resized,
    };
  };

  /**
   * Merges blocks that are in front of i if they are free.
   * If i is the last block, it grows the size to fit the required bytes.
   *
   * @param i The block's index
   * @param bytes The number of bytes we need
   * @returns True if we resized, false otherwise
   */
  auto growForward = [&](uint32_t i, uint32_t bytes) -> bool {
    // If this block is at the end
    // Just update the size
    if (i + 1 == m_Id.size()) {
      m_Size[i] = bytes;
      // If we resized update the total memory used
      m_TotalMemory = m_Offset[i] + m_Size[i];

      // #ifdef DEBUG
      //       std::cout << "Expanding block " << i << " to " << bytes << " bytes" << std::endl;
      // #endif
      // Resized
      return true;
    }

    while (i + 1 < m_Id.size() && m_Free[i + 1]) {
      m_Size[i] += m_Size[i + 1];

      // #ifdef DEBUG
      //       std::cout << "Merging block " << i << " with " << i + 1 << ". Aquired additional " << Size[i + 1] << " bytes" << std::endl;
      // #endif

      m_Id.erase(m_Id.begin() + i + 1);
      m_Size.erase(m_Size.begin() + i + 1);
      m_Offset.erase(m_Offset.begin() + i + 1);
      m_Free.erase(m_Free.begin() + i + 1);
    }

    // Did not resize
    return false;
  };

  // A matching block was found
  if (mi < m_Id.size()) {
    // #ifdef DEBUG
    //     std::cout << "Existing id found" << std::endl;
    // #endif
    // If there is sufficient space in the block that was found
    // assign it and return it.
    if (m_Size[mi] >= bytes) {
      // #ifdef DEBUG
      //       std::cout << "Reusing existing block(" << mi << ")" << std::endl;
      // #endif
      return insertBlock(mi, id, bytes);
    }

    // If there is a block next to this that's free
    auto resized = growForward(mi, bytes);

    // Try to insert the block again after merging
    if (m_Size[mi] >= bytes) {
      // #ifdef DEBUG
      //       std::cout << "Reusing existing block(" << mi << ") after growing" << std::endl;
      // #endif
      return insertBlock(mi, id, bytes, resized);
    }

    // There was not enough space in the matching block
    // Mark the block as free
    m_Id[mi]   = UINT32_MAX;
    m_Free[mi] = true;

    // #ifdef DEBUG
    //     std::cout << "Existing block unusable" << std::endl;
    // #endif
  }

  // No matching block was found
  for (size_t i = 0; i < m_Id.size(); i++) {
    if (!m_Free[i])
      continue;

    // Look for a block that is free and has enough space
    // If we find one, assign it and return it.
    if (m_Size[i] >= bytes) {
      // #ifdef DEBUG
      //       std::cout << "Reusing empty block(" << i << ")" << std::endl;
      // #endif
      return insertBlock(i, id, bytes);
    }

    // If there is a block next to this that's free
    auto resized = growForward(i, bytes);

    // Try to insert the block again after merging
    if (m_Size[i] >= bytes) {
      // #ifdef DEBUG
      //       std::cout << "Reusing empty block(" << i << ") after growing" << std::endl;
      // #endif
      return insertBlock(i, id, bytes, resized);
    }
  }

  // There is no suitable block, create a new one
  uint32_t blocks = m_Id.size();
  uint32_t offset = blocks == 0 ? 0 : m_Offset[blocks - 1] + m_Size[blocks - 1];

  m_Id.emplace_back(id);
  m_Size.emplace_back(bytes);
  m_Offset.emplace_back(offset);
  m_Free.emplace_back(false);

  // Update the total memory used
  size_t last   = m_Id.size() - 1;
  m_TotalMemory = m_Offset[last] + m_Size[last];

  // #ifdef DEBUG
  //   std::cout << "Allocating new block" << std::endl;
  // #endif

  return Allocation {
      .Id      = id,
      .Size    = bytes,
      .Offset  = offset,
      .Resized = true,
  };
}

Allocation Allocator::GetAllocation(uint32_t id) const {
  return Allocation {
      .Id     = m_Id[id],
      .Size   = m_Size[id],
      .Offset = m_Offset[id],
      // Resized is always false here
      .Resized = false,
  };
}

std::vector<Allocation> Allocator::GetAllocations() const {
  std::vector<Allocation> allocations;

  for (size_t i = 0; i < m_Id.size(); i++) {
    if (m_Free[i])
      continue;
    allocations.emplace_back(m_Id[i], m_Size[i], m_Offset[i], false);
  }

  return allocations;
}

std::vector<Allocation> Allocator::GetAllocations(const std::function<bool(uint32_t id)>& resized) const {
  std::vector<Allocation> allocations;

  for (size_t i = 0; i < m_Id.size(); i++) {
    if (m_Free[i])
      continue;
    allocations.emplace_back(m_Id[i], m_Size[i], m_Offset[i], resized(m_Id[i]));
  }

  return allocations;
}

uint32_t Allocator::TotalMemory() const {
  return m_TotalMemory;
}

Allocation Allocator::Check(uint32_t id, uint32_t bytes) const {
  auto mi = m_Id.size();

  for (size_t i = 0; i < m_Id.size(); i++)
    if (m_Id[i] == id) {
      mi = i;
      break;
    }

  auto insertBlock = [&](uint32_t i, uint32_t id, uint32_t bytes, bool resized = false) {
    return Allocation {
        .Id      = id,
        .Size    = bytes,
        .Offset  = m_Offset[i],
        .Resized = resized,
    };
  };

  /**
   * Merges blocks that are in front of i if they are free.
   * If i is the last block, it grows the size to fit the required bytes.
   *
   * @param i The block's index
   * @param bytes The number of bytes we need
   * @returns The new size that would be if we did really grow forward
   */
  auto growForward = [&](uint32_t i, uint32_t bytes, uint32_t& newSize) -> bool {
    // If this block is at the end
    // Just update the size
    if (i + 1 == m_Id.size()) {
      // Resized
      newSize = bytes;
      return true;
    }

    uint32_t currentSize = m_Size[i];

    while (i + 1 < m_Id.size() && m_Free[i + 1])
      currentSize += m_Size[i + 1];

    newSize = currentSize;
    return false;
  };

  if (mi < m_Id.size()) {
    if (m_Size[mi] >= bytes)
      return insertBlock(mi, id, bytes);

    // If there is a block next to this that's free
    uint32_t newSize = 0;
    bool     resized = growForward(mi, bytes, newSize);

    if (newSize >= bytes)
      return insertBlock(mi, id, bytes, resized);
  }

  // No matching block was found
  for (size_t i = 0; i < m_Id.size(); i++) {
    if (!m_Free[i])
      continue;

    if (m_Size[i] >= bytes)
      return insertBlock(i, id, bytes);

    uint32_t newSize = 0;
    bool     resized = growForward(mi, bytes, newSize);

    if (newSize >= bytes)
      return insertBlock(i, id, bytes, resized);
  }

  // There is no suitable block, create a new one
  uint32_t blocks = m_Id.size();
  uint32_t offset = blocks == 0 ? 0 : m_Offset[blocks - 1] + m_Size[blocks - 1];

  return Allocation {
      .Id      = id,
      .Size    = bytes,
      .Offset  = offset,
      .Resized = true,
  };
}

void Allocator::Clear() {
  m_Id.clear();
  m_Size.clear();
  m_Offset.clear();
  m_Free.clear();
}

} // namespace akari::memory
