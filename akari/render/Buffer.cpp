#include "Buffer.h"

#include "window/Application.h"

using namespace akari::window;

namespace akari::render {

void Buffer::CreateDeviceBuffer(uint64_t size, VkCommandBuffer commandBuffer) {

  if (m_DeviceSize >= size)
    return;

  auto* allocator = Application::GetVmaAllocator();

  VkBuffer      previousBuffer     = m_DeviceBuffer;
  VmaAllocation previousAllocation = m_DeviceBufferAllocation;
  uint64_t      previousSize       = m_DeviceSize;

  uint64_t grow   = size - m_DeviceSize;
  uint64_t blocks = (grow + m_Specification.Size - 1) / m_Specification.Size;
  m_DeviceSize += blocks * m_Specification.Size;

#ifdef DEBUG
  std::cout << "Allocating device buffer: " << m_DeviceSize << " bytes" << std::endl;
#endif

  VkBufferCreateInfo bufferInfo {};
  bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size        = m_DeviceSize;
  bufferInfo.usage       = m_Specification.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  if (m_Specification.Pool)
    allocInfo.pool = m_Specification.Pool->GetDevicePool();

  vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_DeviceBuffer, &m_DeviceBufferAllocation, nullptr);

  if (previousBuffer && commandBuffer && previousSize) {
    VkBufferCopy copyRegion {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = previousSize;
    vkCmdCopyBuffer(commandBuffer, previousBuffer, m_DeviceBuffer, 1, &copyRegion);

#ifdef DEBUG
    std::cout << "Copying previous device buffer to new buffer: " << previousSize << " bytes" << std::endl;
#endif

    {
      auto barrier = VkBufferMemoryBarrier2 {
          .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
          .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .dstStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
          .dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer              = m_DeviceBuffer,
          .offset              = 0,
          .size                = VK_WHOLE_SIZE,
      };

      VkDependencyInfo pDependencyInfo {};
      pDependencyInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      pDependencyInfo.bufferMemoryBarrierCount = 1;
      pDependencyInfo.pBufferMemoryBarriers    = &barrier;

      vkCmdPipelineBarrier2(commandBuffer, &pDependencyInfo);
    }
  }

  if (previousBuffer)
    Application::SubmitBufferFree({previousBuffer, previousAllocation});
}

void Buffer::CreateHostBuffer(uint64_t size, VkCommandBuffer commandBuffer) {

  if (m_HostSize >= size)
    return;

  auto* allocator = Application::GetVmaAllocator();

  VkBuffer          previousBuffer         = m_HostBuffer;
  VmaAllocation     previousAllocation     = m_HostBufferAllocation;
  uint64_t          previousSize           = m_HostSize;
  VmaAllocationInfo previousAllocationInfo = m_HostAllocationInfo;

  uint64_t grow   = size - m_HostSize;
  uint64_t blocks = (grow + m_Specification.Size - 1) / m_Specification.Size;
  m_HostSize += blocks * m_Specification.Size;

#ifdef DEBUG
  std::cout << "Allocating host buffer: " << m_HostSize << " bytes" << std::endl;
#endif

  VkBufferCreateInfo bufferInfo {};
  bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size        = m_HostSize;
  bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo {};
  allocInfo.flags         = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  allocInfo.usage         = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
  allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  if (m_Specification.Pool)
    allocInfo.pool = m_Specification.Pool->GetHostPool();

  vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_HostBuffer, &m_HostBufferAllocation, &m_HostAllocationInfo);

  if (previousBuffer && commandBuffer && previousSize) {
    memcpy(m_HostAllocationInfo.pMappedData, previousAllocationInfo.pMappedData, previousSize);

#ifdef DEBUG
    std::cout << "Copying previous host buffer to new buffer: " << previousSize << " bytes" << std::endl;
#endif

    {
      auto barrier = VkBufferMemoryBarrier2 {
          .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
          .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .dstStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
          .dstAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer              = m_HostBuffer,
          .offset              = 0,
          .size                = VK_WHOLE_SIZE,
      };

      VkDependencyInfo pDependencyInfo {};
      pDependencyInfo.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      pDependencyInfo.bufferMemoryBarrierCount = 1;
      pDependencyInfo.pBufferMemoryBarriers    = &barrier;

      vkCmdPipelineBarrier2(commandBuffer, &pDependencyInfo);
    }
  }

  if (previousBuffer)
    Application::SubmitBufferFree({previousBuffer, previousAllocation});
}

Buffer::Allocation Buffer::Allocate(uint64_t id, uint64_t bytes, VkCommandBuffer commandBuffer) {

  Allocation alloc = m_Blocks.Allocate(id, bytes);

  if (alloc.Resized) {
    auto currentSize = m_Blocks.Offset[m_Blocks.Offset.size() - 1] + m_Blocks.Size[m_Blocks.Size.size() - 1];
    CreateHostBuffer(currentSize, commandBuffer);
    CreateDeviceBuffer(currentSize, commandBuffer);
  }

  return alloc;
}

void Buffer::HostToDevice(VkCommandBuffer commandBuffer, const Buffer::Allocation& allocation, const void* data) {
  auto* allocator = Application::GetVmaAllocator();

  vmaCopyMemoryToAllocation(allocator, data, m_HostBufferAllocation, allocation.Offset, allocation.Size);

  VkBufferCopy copyRegion {};
  copyRegion.srcOffset = allocation.Offset;
  copyRegion.dstOffset = allocation.Offset;
  copyRegion.size      = allocation.Size;

#ifdef DEBUG
  std::cout << "Copying from host to device, offset: " << allocation.Offset << ", bytes: " << allocation.Size << std::endl;
#endif

  vkCmdCopyBuffer(commandBuffer, m_HostBuffer, m_DeviceBuffer, 1, &copyRegion);
}

Buffer::Buffer()
    : m_Device(Application::GetDevice())
    , m_PhysicalDevice(Application::GetPhysicalDevice()) {}

Buffer::Buffer(const Specification& specification)
    : m_Device(Application::GetDevice())
    , m_PhysicalDevice(Application::GetPhysicalDevice())
    , m_Specification(specification) {}

Buffer::~Buffer() {
  DestroyBuffer();
}

void Buffer::CreateBuffer(uint64_t size) {
  CreateHostBuffer(size, nullptr);
  CreateDeviceBuffer(size, nullptr);
}

void Buffer::DestroyBuffer() {
  auto* allocator = Application::GetVmaAllocator();

  vmaDestroyBuffer(allocator, m_DeviceBuffer, m_DeviceBufferAllocation);
  vmaDestroyBuffer(allocator, m_HostBuffer, m_HostBufferAllocation);
  m_DeviceSize = 0;
  m_HostSize   = 0;
  m_Blocks.Id.clear();
  m_Blocks.Size.clear();
  m_Blocks.Offset.clear();
  m_Blocks.Free.clear();
}

Buffer::Allocation Buffer::Upload(VkCommandBuffer commandBuffer, uint64_t id, size_t size, const void* data) {
  if (!size)
    return Buffer::Allocation {};

  Buffer::Allocation alloc = Allocate(id, size, commandBuffer);

  HostToDevice(commandBuffer, alloc, data);

  return alloc;
}

VkBufferMemoryBarrier2 Buffer::GetBarrier(VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask) {
  return VkBufferMemoryBarrier2 {
      .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
      .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstStageMask        = dstStageMask,
      .dstAccessMask       = dstAccessMask,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer              = m_DeviceBuffer,
      .offset              = 0,
      .size                = VK_WHOLE_SIZE,
  };
}

Buffer::Allocation Buffer::Blocks::Allocate(uint64_t id, uint64_t bytes) {
#ifdef DEBUG
  std::cout << "\nBuffer::Blocks::Allocate(" << id << ", " << bytes << ")" << std::endl;
#endif

  auto mi = Id.size();

  for (size_t i = 0; i < Id.size(); i++)
    if (Id[i] == id) {
      mi = i;
      break;
    }

  auto insertBlock = [&](uint64_t i, uint64_t id, uint64_t bytes, bool resized = false) {
    // Add a free block with the remaining bytes at position i + 1
    // If there are any left over bytes
    if (Size[i] - bytes) {
      Id.insert(Id.begin() + i + 1, UINT64_MAX);
      Size.insert(Size.begin() + i + 1, Size[i] - bytes);
      Offset.insert(Offset.begin() + i + 1, Offset[i] + bytes);
      Free.insert(Free.begin() + i + 1, true);
    }

    // Update the block at i
    Id[i]   = id;
    Size[i] = bytes;
    Free[i] = false;

    return Allocation {
        .Index   = i,
        .Size    = bytes,
        .Offset  = Offset[i],
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
  auto growForward = [&](uint64_t i, uint64_t bytes) -> bool {
    // If this block is at the end
    // Just update the size
    if (i + 1 == Id.size()) {
      Size[i] = bytes;
#ifdef DEBUG
      std::cout << "Expanding block " << i << " to " << bytes << " bytes" << std::endl;
#endif
      // Resized
      return true;
    }

    while (i + 1 < Id.size() && Free[i + 1]) {
      Size[i] += Size[i + 1];

#ifdef DEBUG
      std::cout << "Merging block " << i << " with " << i + 1 << ". Aquired additional " << Size[i + 1] << " bytes" << std::endl;
#endif

      Id.erase(Id.begin() + i + 1);
      Size.erase(Size.begin() + i + 1);
      Offset.erase(Offset.begin() + i + 1);
      Free.erase(Free.begin() + i + 1);
    }

    // Did not resize
    return false;
  };

  // A matching block was found
  if (mi < Id.size()) {
#ifdef DEBUG
    std::cout << "Existing id found" << std::endl;
#endif
    // If there is sufficient space in the block that was found
    // assign it and return it.
    if (Size[mi] >= bytes) {
#ifdef DEBUG
      std::cout << "Reusing existing block(" << mi << ")" << std::endl;
#endif
      return insertBlock(mi, id, bytes);
    }

    // If there is a block next to this that's free
    auto resized = growForward(mi, bytes);

    // Try to insert the block again after merging
    if (Size[mi] >= bytes) {
#ifdef DEBUG
      std::cout << "Reusing existing block(" << mi << ") after growing" << std::endl;
#endif
      return insertBlock(mi, id, bytes, resized);
    }

    // There was not enough space in the matching block
    // Mark the block as free
    Id[mi]   = UINT64_MAX;
    Free[mi] = true;

#ifdef DEBUG
    std::cout << "Existing block unusable" << std::endl;
#endif
  }

  // No matching block was found
  for (size_t i = 0; i < Id.size(); i++) {
    if (!Free[i])
      continue;

    // Look for a block that is free and has enough space
    // If we find one, assign it and return it.
    if (Size[i] >= bytes) {
#ifdef DEBUG
      std::cout << "Reusing empty block(" << i << ")" << std::endl;
#endif
      return insertBlock(i, id, bytes);
    }

    // If there is a block next to this that's free
    auto resized = growForward(i, bytes);

    // Try to insert the block again after merging
    if (Size[i] >= bytes) {
#ifdef DEBUG
      std::cout << "Reusing empty block(" << i << ") after growing" << std::endl;
#endif
      return insertBlock(i, id, bytes, resized);
    }
  }

  // There is no suitable block, create a new one
  uint64_t blocks = Id.size();
  uint64_t offset = blocks == 0 ? 0 : Offset[blocks - 1] + Size[blocks - 1];

  Id.emplace_back(id);
  Size.emplace_back(bytes);
  Offset.emplace_back(offset);
  Free.emplace_back(false);

#ifdef DEBUG
  std::cout << "Allocating new block" << std::endl;
#endif

  return Allocation {
      .Index   = static_cast<uint64_t>(blocks),
      .Size    = bytes,
      .Offset  = offset,
      .Resized = true,
  };
}

} // namespace akari::render