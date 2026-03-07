#include "BufferPool.h"

#include "window/Application.h"

using namespace akari::window;

namespace akari::render {
BufferPool::BufferPool() {
}

BufferPool::BufferPool(const Specification& specification)
    : m_Specification(specification) {
  auto* allocator = Application::GetVmaAllocator();

  {
    VmaPoolCreateInfo poolInfo {};
    poolInfo.memoryTypeIndex = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    poolInfo.flags           = m_Specification.StagingFlags;
    poolInfo.minBlockCount   = 1;
    poolInfo.maxBlockCount   = 0;

    vmaCreatePool(allocator, &poolInfo, &m_HostPool);
  }

  {
    VmaPoolCreateInfo poolInfo {};
    poolInfo.memoryTypeIndex = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    poolInfo.flags           = m_Specification.DeviceFlags;
    poolInfo.minBlockCount   = 1;
    poolInfo.maxBlockCount   = 0;

    vmaCreatePool(allocator, &poolInfo, &m_DevicePool);
  }
}

BufferPool::~BufferPool() {
  auto* allocator = Application::GetVmaAllocator();
  vmaDestroyPool(allocator, m_HostPool);
  vmaDestroyPool(allocator, m_DevicePool);
}

const VmaPool BufferPool::GetHostPool() const {
  return m_HostPool;
}

const VmaPool BufferPool::GetDevicePool() const {
  return m_DevicePool;
}

} // namespace akari::render
