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

const Buffer::Block& Buffer::Allocate(uint64_t id, uint64_t bytes, bool& outResized, VkCommandBuffer commandBuffer) {
  // for (auto& block : m_Blocks)
  //   if (block.Size >= bytes && block.Free) {
  //     block.Id   = id;
  //     block.Size = bytes;
  //     block.Free = false;
  //     return block;
  //   }

  Block last;

  // if (m_Blocks.size())
  //   last = m_Blocks.back();
  // else
  last = Block {
      .Id     = 0,
      .Size   = 0,
      .Offset = 0,
      .Free   = true,
  };

  uint64_t nextOffset = last.Offset + last.Size;

  bool resizeHostBuffer   = nextOffset + bytes > m_HostSize;
  bool resizeDeviceBuffer = nextOffset + bytes > m_DeviceSize;

  if (resizeHostBuffer)
    CreateHostBuffer(nextOffset + bytes, commandBuffer);

  if (resizeDeviceBuffer)
    CreateDeviceBuffer(nextOffset + bytes, commandBuffer);

  outResized = resizeHostBuffer || resizeDeviceBuffer;

  if(outResized)
    std::cout << "Resized buffer" << std::endl;

  return m_Blocks.emplace_back(Block {
      .Id     = id,
      .Size   = bytes,
      .Offset = nextOffset,
      .Free   = false,
  });
}

void Buffer::HostToDevice(VkCommandBuffer commandBuffer, const Block& block, const void* data) {
  auto* allocator = Application::GetVmaAllocator();

  vmaCopyMemoryToAllocation(allocator, data, m_HostBufferAllocation, block.Offset, block.Size);

  VkBufferCopy copyRegion {};
  copyRegion.srcOffset = block.Offset;
  copyRegion.dstOffset = block.Offset;
  copyRegion.size      = block.Size;

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
  m_Blocks.clear();
}

bool Buffer::Upload(VkCommandBuffer commandBuffer, uint64_t id, size_t size, const void* data) {
  if (!size)
    return false;

  bool resized = false;

  const Block& block = Allocate(id, size, resized, commandBuffer);

  HostToDevice(commandBuffer, block, data);

  return true;
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

} // namespace akari::render