#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace akari::render {

class BufferPool {
public:
  struct Specification {
    VmaPoolCreateFlags StagingFlags {VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT};
    VmaPoolCreateFlags DeviceFlags {VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT};
  };

private:
  Specification m_Specification;
  VmaPool       m_HostPool {nullptr};
  VmaPool       m_DevicePool {nullptr};

public:
  BufferPool();
  BufferPool(const Specification &specification);
  ~BufferPool();

  const VmaPool GetHostPool() const;

  const VmaPool GetDevicePool() const;
};

} // namespace akari::render