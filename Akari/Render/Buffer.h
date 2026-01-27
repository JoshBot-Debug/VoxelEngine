#pragma once

#include <cstring>
#include <vector>
#include <vulkan/vulkan.h>

#include "Application.h"

namespace Akari {
namespace Render {

class Buffer {
private:
  struct Vector {
    uint64_t    count;
    const void* data;
  };

public:
  struct Specification {
    size_t             Size  = 16;
    VkBufferUsageFlags Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  };

private:
  VkDevice         m_Device;
  VkPhysicalDevice m_PhysicalDevice;
  Specification    m_Specification;

  VkBuffer m_Buffer        = VK_NULL_HANDLE;
  VkBuffer m_StagingBuffer = VK_NULL_HANDLE;

  VmaAllocation m_BufferAllocation        = VK_NULL_HANDLE;
  VmaAllocation m_StagingBufferAllocation = VK_NULL_HANDLE;

  VkDeviceSize m_Size = 0;

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

  template <typename T>
  bool Upload(VkCommandBuffer commandBuffer, const std::vector<T>& vector) {

    std::vector<uint8_t> buffer = BuildVector(vector);

    size_t size = buffer.size();

    if (m_Size >= size) {
      CopyToGPU(commandBuffer, size, buffer.data());
      return false;
    }

    VkBuffer      oBuffer                  = m_Buffer;
    VkBuffer      oStagingBuffer           = m_StagingBuffer;
    VmaAllocation oBufferAllocation        = m_BufferAllocation;
    VmaAllocation oStagingBufferAllocation = m_StagingBufferAllocation;

    CreateBuffer(size);

    CopyToGPU(commandBuffer, size, buffer.data());

    vmaDestroyBuffer(Akari::Application::GetVmaAllocator(), oBuffer,
                     oBufferAllocation);
    vmaDestroyBuffer(Akari::Application::GetVmaAllocator(), oStagingBuffer,
                     oStagingBufferAllocation);

    return true;
  }

  bool Upload(VkCommandBuffer commandBuffer, size_t size, void* data);

  VkBuffer GetBuffer() const { return m_Buffer; }

  VkBufferMemoryBarrier2 GetBarrier(VkPipelineStageFlags2 dstStageMask,
                                    VkAccessFlags2        dstAccessMask);

  /// Returns device address (for bindless shaders)
  VkDeviceAddress GetDeviceAddress() const;
};

} // namespace Render
} // namespace Akari
