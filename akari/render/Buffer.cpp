#include "Buffer.h"

#include "window/Application.h"
#include <algorithm>

using namespace akari::window;

namespace akari::render {

void Buffer::CreateDeviceBuffer(uint32_t size, VkCommandBuffer commandBuffer) {

  if (m_DeviceSize >= size)
    return;

  auto* allocator = Application::GetVmaAllocator();

  VkBuffer      previousBuffer     = m_DeviceBuffer;
  VmaAllocation previousAllocation = m_DeviceBufferAllocation;
  uint32_t      previousSize       = m_DeviceSize;

  uint32_t grow   = size - m_DeviceSize;
  uint32_t blocks = (grow + m_Specification.Size - 1) / m_Specification.Size;
  m_DeviceSize += blocks * m_Specification.Size;

  // #ifdef DEBUG
  //   std::cout << "Allocating device buffer: " << m_DeviceSize << " bytes" << std::endl;
  // #endif

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

#ifdef ENABLE_VULKAN_VALIDATION
  if (!m_Specification.DebugName.empty()) {
    SET_DEBUG_OBJECT_NAME(
        m_Device,
        VK_OBJECT_TYPE_BUFFER,
        (uint64_t)(m_DeviceBuffer),
        (m_Specification.DebugName + " m_DeviceBuffer").c_str());
  }
#endif

  if (previousBuffer && commandBuffer && previousSize) {
    VkBufferCopy copyRegion {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = previousSize;
    vkCmdCopyBuffer(commandBuffer, previousBuffer, m_DeviceBuffer, 1, &copyRegion);

    // #ifdef DEBUG
    //     std::cout << "Copying previous device buffer to new buffer: " << previousSize << " bytes" << std::endl;
    // #endif

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
    Application::FreeBuffer({previousBuffer, previousAllocation});
}

void Buffer::CreateHostBuffer(uint32_t size, VkCommandBuffer commandBuffer) {

  if (m_HostSize >= size)
    return;

  auto* allocator = Application::GetVmaAllocator();

  VkBuffer          previousBuffer         = m_HostBuffer;
  VmaAllocation     previousAllocation     = m_HostBufferAllocation;
  uint32_t          previousSize           = m_HostSize;
  VmaAllocationInfo previousAllocationInfo = m_HostAllocationInfo;

  uint32_t grow   = size - m_HostSize;
  uint32_t blocks = (grow + m_Specification.Size - 1) / m_Specification.Size;
  m_HostSize += blocks * m_Specification.Size;

  // #ifdef DEBUG
  //   std::cout << "Allocating host buffer: " << m_HostSize << " bytes" << std::endl;
  // #endif

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

#ifdef ENABLE_VULKAN_VALIDATION
  if (!m_Specification.DebugName.empty()) {
    SET_DEBUG_OBJECT_NAME(
        m_Device,
        VK_OBJECT_TYPE_BUFFER,
        (uint64_t)(m_HostBuffer),
        (m_Specification.DebugName + " m_HostBuffer").c_str());
  }
#endif

  if (previousBuffer && commandBuffer && previousSize) {
    memcpy(m_HostAllocationInfo.pMappedData, previousAllocationInfo.pMappedData, previousSize);

    // #ifdef DEBUG
    //     std::cout << "Copying previous host buffer to new buffer: " << previousSize << " bytes" << std::endl;
    // #endif

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
    Application::FreeBuffer({previousBuffer, previousAllocation});
}

Allocation Buffer::Allocate(uint32_t id, uint32_t bytes, VkCommandBuffer commandBuffer) {

  Allocation alloc = m_Allocator.Allocate(id, bytes);

  if (alloc.Resized) {
    auto currentSize = m_Allocator.TotalMemory();
    CreateHostBuffer(currentSize, commandBuffer);
    CreateDeviceBuffer(currentSize, commandBuffer);
  }

  return alloc;
}

void Buffer::HostToDevice(VkCommandBuffer commandBuffer, const Allocation& allocation, const void* data) {
  auto* allocator = Application::GetVmaAllocator();

  vmaCopyMemoryToAllocation(allocator, data, m_HostBufferAllocation, allocation.Offset, allocation.Size);

  VkBufferCopy copyRegion {};
  copyRegion.srcOffset = allocation.Offset;
  copyRegion.dstOffset = allocation.Offset;
  copyRegion.size      = allocation.Size;

  // #ifdef DEBUG
  //   std::cout << "Copying from host to device, offset: " << allocation.Offset << ", bytes: " << allocation.Size << std::endl;
  // #endif

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

void Buffer::CreateBuffer() {
  CreateHostBuffer(m_Specification.Size, nullptr);
  CreateDeviceBuffer(m_Specification.Size, nullptr);
}

void Buffer::CreateBuffer(const Buffer::Specification& specification) {
  m_Specification = specification;
  CreateHostBuffer(specification.Size, nullptr);
  CreateDeviceBuffer(specification.Size, nullptr);
}

void Buffer::DestroyBuffer() {
  auto* allocator = Application::GetVmaAllocator();

  vmaDestroyBuffer(allocator, m_DeviceBuffer, m_DeviceBufferAllocation);
  vmaDestroyBuffer(allocator, m_HostBuffer, m_HostBufferAllocation);
  m_DeviceSize = 0;
  m_HostSize   = 0;
  m_Allocator.Clear();
}

Allocation Buffer::Upload(VkCommandBuffer commandBuffer, uint32_t id, size_t size, const void* data) {
  if (!size)
    return Allocation {};

  Allocation alloc = Allocate(id, size, commandBuffer);

  HostToDevice(commandBuffer, alloc, data);

  return alloc;
}

std::vector<Allocation> Buffer::Upload(VkCommandBuffer commandBuffer, const std::vector<Partition>& partitions, const void* data) {

  auto*                   allocator = Application::GetVmaAllocator();
  bool                    resized {false};
  std::vector<Allocation> allocations {partitions.size()};

  for (size_t i = 0; i < partitions.size(); i++) {
    allocations[i] = m_Allocator.Allocate(partitions[i].Id, partitions[i].Size);
    if (allocations[i].Resized)
      resized = true;
  }

  if (resized) {
    auto currentSize = m_Allocator.TotalMemory();
    CreateHostBuffer(currentSize, commandBuffer);
    CreateDeviceBuffer(currentSize, commandBuffer);
  }

  uint32_t sourceOffset = 0;
  for (size_t i = 0; i < partitions.size(); i++) {
    auto& partition  = partitions[i];
    auto& allocation = allocations[i];

    const auto* src = static_cast<const uint8_t*>(data) + sourceOffset;

    vmaCopyMemoryToAllocation(allocator, src, m_HostBufferAllocation, allocation.Offset, allocation.Size);

    VkBufferCopy copyRegion {};
    copyRegion.srcOffset = allocation.Offset;
    copyRegion.dstOffset = allocation.Offset;
    copyRegion.size      = allocation.Size;

    vkCmdCopyBuffer(commandBuffer, m_HostBuffer, m_DeviceBuffer, 1, &copyRegion);

    sourceOffset += partition.Size;
  }

  auto hasResized = [&](uint32_t id) -> bool {
    auto it = std::find_if(allocations.begin(), allocations.end(), [id](Allocation& a) {
      return a.Id == id;
    });
    if (it == allocations.end())
      return false;
    return it->Resized;
  };

  return m_Allocator.GetAllocations(hasResized);
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
