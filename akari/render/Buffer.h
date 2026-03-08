#pragma once

#include <cstring>
#include <mutex>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "BufferPool.h"

namespace akari::render {

class Buffer {

public:
  struct Allocation {
    uint64_t Index {UINT64_MAX};
    uint64_t Size {0};
    uint64_t Offset {0};
    bool     Resized {false};
  };

  struct Blocks {
    std::vector<uint64_t> Id {};
    std::vector<uint64_t> Size {};
    std::vector<uint64_t> Offset {};
    std::vector<bool>     Free {};

    Allocation Allocate(uint64_t id, uint64_t bytes);

    void Resize(uint64_t bytes);
  };

  struct Specification {
    /// TODO: There is a glitch every time we need to resize the buffer and copy content.
    // uint64_t           Size {1024 * 1024 * 64};
    uint64_t           Size {1024 * 1024 * 64};
    VkBufferUsageFlags Usage {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
    BufferPool*        Pool {nullptr};
  };

private:
  Blocks m_Blocks {};

  VkDevice         m_Device;
  VkPhysicalDevice m_PhysicalDevice;
  Specification    m_Specification;

  VkBuffer m_DeviceBuffer {VK_NULL_HANDLE};
  VkBuffer m_HostBuffer {VK_NULL_HANDLE};

  VmaAllocation m_DeviceBufferAllocation {VK_NULL_HANDLE};
  VmaAllocation m_HostBufferAllocation {VK_NULL_HANDLE};

  VkDeviceSize m_DeviceSize {0};
  VkDeviceSize m_HostSize {0};

private:
  /**
   * Creates a device buffer of a specified size
   *
   * @note Implicitly destroys the previous buffer if any
   */
  void CreateDeviceBuffer(uint64_t size, VkCommandBuffer commandBuffer);

  /**
   * Creates the host buffer.
   * If the specification size is 256mb the buffer is created with it's current size + 256mb
   *
   * @note Implicitly destroys the previous buffer if any
   */
  void CreateHostBuffer(uint64_t size, VkCommandBuffer commandBuffer);

  template <typename T>
  std::vector<uint8_t> BuildStorageBufferAlignedData(const std::vector<T>& src) {

    uint32_t count = static_cast<uint32_t>(src.size());

    size_t size = count * sizeof(T);

    // Need 16 byte alignment
    uint32_t padding = size % 16;

    std::vector<uint8_t> buffer(16 + size + padding, 0);

    std::memcpy(buffer.data(), &count, sizeof(count));

    // Copy elements starting at offset 16 (for 16 byte alignment)
    std::memcpy(buffer.data() + 16, src.data(), size);

    return buffer;
  }

  /**
   * Allocates a block & allocates more memory in the host & device buffer if needed.
   */
  Allocation Allocate(uint64_t id, uint64_t bytes, VkCommandBuffer commandBuffer);

  void HostToDevice(VkCommandBuffer commandBuffer, uint64_t block, const void* data);

public:
  Buffer();
  Buffer(const Specification& specification);
  ~Buffer();

  Buffer(const Buffer& allocator)            = delete;
  Buffer& operator=(const Buffer& allocator) = delete;

  void CreateBuffer(uint64_t size = 0);

  void DestroyBuffer();

  void SetPool(BufferPool* pool) { m_Specification.Pool = pool; };

  /**
   * For storage buffers & vector data only.
   */
  template <typename T>
  Buffer::Allocation Upload(VkCommandBuffer commandBuffer, uint64_t id, const std::vector<T>& vector) {
    if (!vector.size())
      return Buffer::Allocation {};
    std::vector<uint8_t> buffer = BuildStorageBufferAlignedData(vector);
    return Upload(commandBuffer, id, buffer.size(), buffer.data());
  }

  /**
   * For storage buffers & vector data only.
   */
  template <typename T>
  Buffer::Allocation Upload(VkCommandBuffer commandBuffer, const std::vector<T>& vector) {
    return Upload(commandBuffer, 0, vector);
  }

  Buffer::Allocation Upload(VkCommandBuffer commandBuffer, uint64_t id, size_t size, const void* data);

  Buffer::Allocation Upload(VkCommandBuffer commandBuffer, size_t size, const void* data) {
    return Upload(commandBuffer, 0, size, data);
  }

  VkBufferMemoryBarrier2 GetBarrier(VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

  VkBuffer GetBuffer() { return m_DeviceBuffer; }
};

} // namespace akari::render
