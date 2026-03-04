#pragma once

#include <cstring>
#include <vector>
#include <vulkan/vulkan.h>

#include "window/Application.h"

namespace akari::render {

class BufferPool;

class Buffer {
private:
  struct Vector {
    uint64_t    count;
    const void* data;
  };

public:
  struct Specification {
    size_t             Size {1024};
    VkBufferUsageFlags Usage {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
    BufferPool*        Pool {nullptr};
  };

private:
  VkDevice         m_Device;
  VkPhysicalDevice m_PhysicalDevice;
  Specification    m_Specification;

  VkBuffer m_DeviceBuffer {VK_NULL_HANDLE};
  VkBuffer m_HostBuffer {VK_NULL_HANDLE};

  VmaAllocation m_DeviceBufferAllocation {VK_NULL_HANDLE};
  VmaAllocation m_HostBufferAllocation {VK_NULL_HANDLE};

  VkDeviceSize m_Size {0};

private:
  void DestroyBuffer();

  void CopyToGPU(VkCommandBuffer commandBuffer, size_t size, const void* data);

  template <typename T>
  std::vector<uint8_t> BuildVector(const std::vector<T>& src) {

    uint32_t count = static_cast<uint32_t>(src.size());

    // Size of the data
    size_t size = count * sizeof(T);

    // Need 16 byte alignment
    uint32_t padding = size % 16;

    // 16 bytes header + sizeof elements + padding
    std::vector<uint8_t> buffer(16 + size + padding, 0);

    // Write element count at offset 0
    std::memcpy(buffer.data(), &count, sizeof(count));

    // Copy elements starting at offset 16
    std::memcpy(buffer.data() + 16, src.data(), size);

    return buffer;
  }

public:
  Buffer();
  Buffer(const Specification& specification);
  ~Buffer();

  void CreateBuffer(VkDeviceSize size);

  /**
   * Uploads a vector of structs, with size as the first arg
   * For vectors & storage buffers only
   *
   * @note This method is for storage buffers where the data is AOS.
   * @returns True if the buffer resized, False otherwise
   */
  template <typename T>
  bool Upload(VkCommandBuffer commandBuffer, const std::vector<T>& vector) {
    if (!vector.size())
      return false;
    std::vector<uint8_t> buffer = BuildVector(vector);
    return Upload(commandBuffer, buffer.size(), buffer.data());
  }

  /**
   * Uploads any data to a buffer that's visible on the GPU
   *
   * @note This is for arbitrary data.
   * @returns True if the buffer resized, False otherwise
   */
  bool Upload(VkCommandBuffer commandBuffer, size_t size, const void* data);

  VkBuffer GetBuffer() const { return m_DeviceBuffer; }

  VkBufferMemoryBarrier2 GetBarrier(VkPipelineStageFlags2 dstStageMask,
                                    VkAccessFlags2        dstAccessMask);

  /// Returns device address (for bindless shaders)
  VkDeviceAddress GetDeviceAddress() const;

  void SetPool(BufferPool* pool) { m_Specification.Pool = pool; }
};

} // namespace akari::render
