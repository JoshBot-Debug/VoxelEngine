#pragma once

#include <cstring>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "BufferPool.h"

namespace akari::render {

class Buffer {

public:
  struct Partition {
    uint32_t Id;
    size_t   Size;
  };

  struct Allocation {
    uint32_t Id {0};
    uint32_t Size {0};
    uint32_t Offset {0};
    bool     Resized {false};
  };

  struct Blocks {
    std::vector<uint32_t> Id {};
    std::vector<uint32_t> Size {};
    std::vector<uint32_t> Offset {};
    std::vector<bool>     Free {};

    Allocation Allocate(uint32_t id, uint32_t bytes);

    void Log();
  };

  struct Specification {
    uint32_t           Size {1024 * 32};
    VkBufferUsageFlags Usage {VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
    BufferPool*        Pool {nullptr};
    std::string        DebugName {};
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

  VmaAllocationInfo m_HostAllocationInfo;

  std::string m_DebugName = "";

private:
  /**
   * Creates a device buffer of a specified size
   *
   * @note Implicitly destroys the previous buffer if any
   */
  void CreateDeviceBuffer(uint32_t size, VkCommandBuffer commandBuffer);

  /**
   * Creates the host buffer.
   * If the specification size is 256mb the buffer is created with it's current size + 256mb
   *
   * @note Implicitly destroys the previous buffer if any
   */
  void CreateHostBuffer(uint32_t size, VkCommandBuffer commandBuffer);

  /**
   * Ensures 16 byte alignment for storage buffers and prepends the total count.
   * Used to pass vectors to a gpu
   */
  template <typename T>
  std::vector<uint8_t> BuildStorageBufferAlignedData(const std::vector<T>& src);

  /**
   * Ensures 16 byte alignment for storage buffers and prepends the total count.
   * Used to pass vectors to a gpu
   */
  template <typename T>
  std::vector<uint8_t> BuildStorageBufferAlignedData(const std::vector<Partition>& partitions, const std::vector<T>& src);

  /**
   * Allocates a block & allocates more memory in the host & device buffer if needed.
   */
  Allocation Allocate(uint32_t id, uint32_t bytes, VkCommandBuffer commandBuffer);

  void HostToDevice(VkCommandBuffer commandBuffer, const Buffer::Allocation& allocation, const void* data);

public:
  Buffer();
  Buffer(const Specification& specification);
  ~Buffer();

  Buffer(const Buffer& allocator)            = delete;
  Buffer& operator=(const Buffer& allocator) = delete;

  void CreateBuffer();

  void CreateBuffer(const Specification& specification);

  void DestroyBuffer();

  void SetPool(BufferPool* pool) { m_Specification.Pool = pool; };

  /**
   * Uploads a contiguous vector of data into a GPU storage buffer with an explicit ID.
   * Only valid for storage buffer usage.
   *
   * @tparam T            Type of elements in the vector.
   * @param commandBuffer Command buffer used to record the transfer operation.
   * @param id            Identifier used to group or track this allocation.
   * @param vector        Source data to upload.
   * @return              Allocation handle describing the uploaded buffer region.
   */
  template <typename T>
  Buffer::Allocation Upload(VkCommandBuffer commandBuffer, uint32_t id, const std::vector<T>& vector);

  /**
   * Uploads a contiguous vector of data into a GPU storage buffer.
   * Uses a default ID (0). Only valid for storage buffer usage.
   *
   * @tparam T            Type of elements in the vector.
   * @param commandBuffer Command buffer used to record the transfer operation.
   * @param vector        Source data to upload.
   * @return              Allocation handle describing the uploaded buffer region.
   */
  template <typename T>
  Buffer::Allocation Upload(VkCommandBuffer commandBuffer, const std::vector<T>& vector);

  /**
   * Uploads a vector of data segmented per partition into GPU storage buffers.
   * Each partition receives a corresponding allocation.
   * Only valid for storage buffer usage.
   *
   * @tparam T            Type of elements in the vector.
   * @param commandBuffer Command buffer used to record the transfer operations.
   * @param partitions    Partitions metadata defining how data is partitioned.
   * @param vector        Source data to upload.
   * @return              List of allocation handles, one per partition.
   */
  template <typename T>
  std::vector<Buffer::Allocation> Upload(VkCommandBuffer commandBuffer, const std::vector<Partition>& partitions, const std::vector<T>& vector);

  /**
   * Uploads raw data into a GPU buffer with an explicit ID.
   * For byte-level data (not limited to storage buffers).
   *
   * @param commandBuffer Command buffer used to record the transfer operation.
   * @param id            Identifier used to group or track this allocation.
   * @param size          Size of the data in bytes.
   * @param data          Pointer to the source data.
   * @return              Allocation handle describing the uploaded buffer region.
   */
  Buffer::Allocation Upload(VkCommandBuffer commandBuffer, uint32_t id, size_t size, const void* data);

  /**
   * Uploads raw data segmented per partition into GPU buffers.
   * Each partition receives a corresponding allocation.
   * Suitable for unstructured or byte-level data.
   *
   * @param commandBuffer Command buffer used to record the transfer operations.
   * @param partitions    Partition metadata defining how data is partitioned.
   * @param data          Pointer to the source data.
   * @return              List of allocation handles, one per partition.
   */
  std::vector<Buffer::Allocation> Upload(VkCommandBuffer commandBuffer, const std::vector<Partition>& partitions, const void* data);

  /**
   * Uploads raw data into a GPU buffer using the default ID (0).
   *
   * @param commandBuffer Command buffer used to record the transfer operation.
   * @param size          Size of the data in bytes.
   * @param data          Pointer to the source data.
   * @return              Allocation handle describing the uploaded buffer region.
   */
  Buffer::Allocation Upload(VkCommandBuffer commandBuffer, size_t size, const void* data) {
    return Upload(commandBuffer, 0, size, data);
  }

  VkBufferMemoryBarrier2 GetBarrier(VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

  VkBuffer GetBuffer() { return m_DeviceBuffer; }

  void Log();
};

template <typename T>
inline std::vector<uint8_t> Buffer::BuildStorageBufferAlignedData(const std::vector<T>& src) {

  uint32_t length = static_cast<uint32_t>(src.size());

  size_t bytes = length * sizeof(T);

  // Need 16 byte alignment
  uint32_t padding = (16 - (bytes % 16)) % 16;

  std::vector<uint8_t> buffer(16 + bytes + padding, 0);

  std::memcpy(buffer.data(), &length, sizeof(length));

  // Copy elements starting at offset 16 (for 16 byte alignment)
  std::memcpy(buffer.data() + 16, src.data(), bytes);

  return buffer;
}

template <typename T>
inline std::vector<uint8_t> Buffer::BuildStorageBufferAlignedData(const std::vector<Partition>& partitions, const std::vector<T>& src) {

  size_t               structBytes = sizeof(T);
  uint32_t             offset      = 0;
  std::vector<uint8_t> buffer;
  uint32_t             partitionLength = static_cast<uint32_t>(partitions.size());

  for (uint32_t i = 0; i < partitionLength; i++) {
    auto&    partition              = partitions[i];
    uint32_t bytes                  = partition.Size;
    uint32_t length                 = bytes / structBytes;
    uint32_t padding                = (16 - (bytes % 16)) % 16;
    size_t   writePosition          = buffer.size();
    uint32_t additionalSpace        = 16 + bytes + padding;
    uint32_t nextPartitionItemIndex = offset + length;
    uint32_t zero                   = 0;
    const T* srcPtr                 = &src[offset];

    buffer.resize(writePosition + additionalSpace, 0);

    uint8_t* ptr = buffer.data() + writePosition;

    std::memcpy(ptr + 0, &length, sizeof(length));
    std::memcpy(ptr + 4, &nextPartitionItemIndex, sizeof(nextPartitionItemIndex));
    std::memcpy(ptr + 8, &partitionLength, sizeof(partitionLength));
    std::memcpy(ptr + 12, &zero, sizeof(zero));
    std::memcpy(ptr + 16, srcPtr, bytes);

    offset += length;
  }

  return buffer;
}

template <typename T>
inline Buffer::Allocation Buffer::Upload(VkCommandBuffer commandBuffer, uint32_t id, const std::vector<T>& vector) {
  if (!vector.size())
    return Buffer::Allocation {};
  std::vector<uint8_t> buffer = BuildStorageBufferAlignedData(vector);
  return Upload(commandBuffer, id, buffer.size(), buffer.data());
}

template <typename T>
inline Buffer::Allocation Buffer::Upload(VkCommandBuffer commandBuffer, const std::vector<T>& vector) {
  return Upload(commandBuffer, 0, vector);
}

template <typename T>
inline std::vector<Buffer::Allocation> Buffer::Upload(VkCommandBuffer commandBuffer, const std::vector<Partition>& partitions, const std::vector<T>& vector) {
  if (!vector.size())
    return std::vector<Buffer::Allocation> {};
  std::vector<uint8_t> buffer = BuildStorageBufferAlignedData(partitions, vector);
  return Upload(commandBuffer, partitions, buffer.data());
}

} // namespace akari::render
