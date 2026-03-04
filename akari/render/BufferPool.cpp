#include "BufferPool.h"

#include "window/Application.h"

namespace akari::render {

uint32_t BufferPool::FindMemoryTypeIndex(VmaAllocator allocator, VmaMemoryUsage usage, VkBufferUsageFlags bufferUsage) {
  VkBufferCreateInfo bufferInfo {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size  = 1;
  bufferInfo.usage = bufferUsage;

  VmaAllocationCreateInfo allocInfo {};
  allocInfo.usage = usage;

  uint32_t memTypeIndex;
  vmaFindMemoryTypeIndexForBufferInfo(allocator, &bufferInfo, &allocInfo, &memTypeIndex);
  return memTypeIndex;
}

BufferPool::BufferPool(const Specification& specification) {
  m_Specification = specification;
  auto* allocator = akari::window::Application::GetVmaAllocator();

  {
    VmaPoolCreateInfo poolInfo {};
    poolInfo.memoryTypeIndex = FindMemoryTypeIndex(allocator, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    poolInfo.flags           = m_Specification.StagingFlags;
    poolInfo.minBlockCount   = 1;
    poolInfo.maxBlockCount   = 0;

    vmaCreatePool(allocator, &poolInfo, &m_HostPool);
  }

  {
    VmaPoolCreateInfo poolInfo {};
    poolInfo.memoryTypeIndex = FindMemoryTypeIndex(allocator, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VK_BUFFER_USAGE_TRANSFER_DST_BIT | m_Specification.Usage);
    poolInfo.flags           = m_Specification.DeviceFlags;
    poolInfo.minBlockCount   = 1;
    poolInfo.maxBlockCount   = 0;

    vmaCreatePool(allocator, &poolInfo, &m_DevicePool);
  }
}

BufferPool::~BufferPool() {
  vmaDestroyPool(akari::window::Application::GetVmaAllocator(), m_HostPool);
  vmaDestroyPool(akari::window::Application::GetVmaAllocator(), m_DevicePool);
}

const VmaPool BufferPool::GetHostPool() const {
  return m_HostPool;
}

const VmaPool BufferPool::GetDevicePool() const {
  return m_DevicePool;
}

} // namespace akari::render
