#include "Buffer.h"

#include "window/Application.h"

#include "BufferPool.h"
#include <iostream>

namespace akari::render {

Buffer::Buffer() {
  m_Device         = akari::window::Application::GetDevice();
  m_PhysicalDevice = akari::window::Application::GetPhysicalDevice();
}

Buffer::Buffer(const Specification& specification)
    : m_Specification(specification) {
  m_Device         = akari::window::Application::GetDevice();
  m_PhysicalDevice = akari::window::Application::GetPhysicalDevice();
}

Buffer::~Buffer() {
  DestroyBuffer();
}

bool Buffer::Upload(VkCommandBuffer commandBuffer, size_t size, const void* data) {
  if (!size)
    return false;

  if (m_Size >= size) {
    CopyToGPU(commandBuffer, size, data);
    return false;
  }

  VkBuffer      oBuffer                  = m_DeviceBuffer;
  VkBuffer      oStagingBuffer           = m_HostBuffer;
  VmaAllocation oBufferAllocation        = m_DeviceBufferAllocation;
  VmaAllocation oStagingBufferAllocation = m_HostBufferAllocation;

  CreateBuffer(size);

  CopyToGPU(commandBuffer, size, data);

  vmaDestroyBuffer(akari::window::Application::GetVmaAllocator(), oBuffer, oBufferAllocation);
  vmaDestroyBuffer(akari::window::Application::GetVmaAllocator(), oStagingBuffer, oStagingBufferAllocation);

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

VkDeviceAddress Buffer::GetDeviceAddress() const {
  VkBufferDeviceAddressInfo addressInfo {};
  addressInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  addressInfo.buffer = m_DeviceBuffer;
  return vkGetBufferDeviceAddress(m_Device, &addressInfo);
}

void Buffer::CreateBuffer(VkDeviceSize size) {

  m_Size = size;

  // Staging Buffer
  {
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = m_Size;
    bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    if (m_Specification.Pool)
      allocInfo.pool = m_Specification.Pool->GetHostPool();

    vmaCreateBuffer(akari::window::Application::GetVmaAllocator(), &bufferInfo, &allocInfo, &m_HostBuffer, &m_HostBufferAllocation, nullptr);
  }

  // GPU Buffer
  {
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = m_Size;
    bufferInfo.usage       = m_Specification.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (m_Specification.Pool)
      allocInfo.pool = m_Specification.Pool->GetDevicePool();

    vmaCreateBuffer(akari::window::Application::GetVmaAllocator(), &bufferInfo, &allocInfo, &m_DeviceBuffer, &m_DeviceBufferAllocation, nullptr);
  }
}

void Buffer::DestroyBuffer() {
  vmaDestroyBuffer(akari::window::Application::GetVmaAllocator(), m_DeviceBuffer, m_DeviceBufferAllocation);
  vmaDestroyBuffer(akari::window::Application::GetVmaAllocator(), m_HostBuffer, m_HostBufferAllocation);

  m_Size = 0;
}

void Buffer::CopyToGPU(VkCommandBuffer commandBuffer, size_t size, const void* data) {
  vmaCopyMemoryToAllocation(akari::window::Application::GetVmaAllocator(), data, m_HostBufferAllocation, 0, size);

  VkBufferCopy copyRegion {};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size      = size;

  vkCmdCopyBuffer(commandBuffer, m_HostBuffer, m_DeviceBuffer, 1, &copyRegion);
}
} // namespace akari::render
