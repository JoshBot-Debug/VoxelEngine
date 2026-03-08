#include "Buffer.h"

#include "window/Application.h"

using namespace akari::window;

namespace akari::render {

void Buffer::CreateDeviceBuffer(uint64_t size, VkCommandBuffer commandBuffer) {
  auto* allocator = Application::GetVmaAllocator();

  VkBuffer      previousBuffer     = m_DeviceBuffer;
  VmaAllocation previousAllocation = m_DeviceBufferAllocation;
  uint64_t      previousSize       = m_DeviceSize;

  uint64_t grow   = size - m_DeviceSize;
  uint64_t blocks = (grow + m_Specification.Size - 1) / m_Specification.Size;
  m_DeviceSize += blocks * m_Specification.Size;

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
  auto* allocator = Application::GetVmaAllocator();

  VkBuffer      previousBuffer     = m_HostBuffer;
  VmaAllocation previousAllocation = m_HostBufferAllocation;
  uint64_t      previousSize       = m_HostSize;

  uint64_t grow   = size - m_HostSize;
  uint64_t blocks = (grow + m_Specification.Size - 1) / m_Specification.Size;
  m_HostSize += blocks * m_Specification.Size;

  VkBufferCreateInfo bufferInfo {};
  bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size        = m_HostSize;
  bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo {};
  allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

  if (m_Specification.Pool)
    allocInfo.pool = m_Specification.Pool->GetHostPool();

  vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_HostBuffer, &m_HostBufferAllocation, nullptr);

  if (previousBuffer && commandBuffer && previousSize) {
    VkBufferCopy copyRegion {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = previousSize;
    vkCmdCopyBuffer(commandBuffer, previousBuffer, m_HostBuffer, 1, &copyRegion);

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

  std::cout << alloc.Index << " " << alloc.Resized << " bytes: " << bytes << std::endl;

  uint64_t offset = m_Blocks.Offset[alloc.Index];
  uint64_t size   = m_Blocks.Size[alloc.Index];

  if (alloc.Resized) {
    uint64_t nSize = offset + size + bytes;
    CreateHostBuffer(nSize, commandBuffer);
    CreateDeviceBuffer(nSize, commandBuffer);
  }

  return alloc;
}

void Buffer::HostToDevice(VkCommandBuffer commandBuffer, uint64_t block, const void* data) {
  auto* allocator = Application::GetVmaAllocator();

  auto offset = m_Blocks.Offset[block];
  auto size   = m_Blocks.Size[block];

  vmaCopyMemoryToAllocation(allocator, data, m_HostBufferAllocation, offset, size);

  VkBufferCopy copyRegion {};
  copyRegion.srcOffset = offset;
  copyRegion.dstOffset = offset;
  copyRegion.size      = size;

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
  // m_Blocks.Resize(m_DeviceSize);
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

  HostToDevice(commandBuffer, alloc.Index, data);

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
  auto blocks = Id.size();

  auto mi = blocks;

  for (size_t i = 0; i < blocks; i++)
    if (Id[i] == id) {
      mi = i;
      break;
    }

  // A matching block was found
  if (mi < blocks) {

    // If there is sufficient space in the block that was found
    // assign it and return it.
    if (Size[mi] >= bytes) {
      Size[mi] = bytes;
      Free[mi] = false;

      std::cout << "Reused (existing id)" << std::endl;
      return Allocation {
          .Index  = mi,
          .Size   = bytes,
          .Offset = Offset[mi],
      };
    }

    // There was not enough space in the matching block
    // Mark the block as free
    Id[mi]   = UINT64_MAX;
    Free[mi] = true;
  }

  // No matching block was found
  for (size_t i = 0; i < blocks; i++)
    // Look for a block that is free and has enough space
    // If we find one, assign it and return it.
    if (Size[i] >= bytes && Free[i]) {
      Id[i]   = id;
      Size[i] = bytes;
      Free[i] = false;

      std::cout << "Reused (empty block)" << std::endl;
      return Allocation {
          .Index  = i,
          .Size   = bytes,
          .Offset = Offset[i],
      };
    }

  // There is no suitable block, create a new one
  uint64_t offset = blocks == 0 ? 0 : Offset[blocks - 1] + Size[blocks - 1];

  Id.emplace_back(id);
  Size.emplace_back(bytes);
  Offset.emplace_back(offset);
  Free.emplace_back(false);

  std::cout << "Created (new block)" << std::endl;
  return Allocation {
      .Index   = static_cast<uint64_t>(blocks),
      .Size    = bytes,
      .Offset  = offset,
      .Resized = true,
  };
}

void Buffer::Blocks::Resize(uint64_t bytes) {
  auto blocks = Id.size();

  if (blocks == 0) {
    Id.emplace_back(UINT64_MAX);
    Size.emplace_back(bytes);
    Offset.emplace_back(0);
    Free.emplace_back(true);
    return;
  }

  auto     index  = blocks - 1;
  uint64_t offset = Offset[index];
  uint64_t size   = Size[index];
  uint64_t nSize  = offset + size + bytes;

  Id.emplace_back(UINT64_MAX);
  Size.emplace_back(nSize);
  Offset.emplace_back(offset + size);
  Free.emplace_back(false);
}

} // namespace akari::render