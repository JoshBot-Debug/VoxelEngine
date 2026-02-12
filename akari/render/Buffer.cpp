#include "Buffer.h"

#include "window/Application.h"

#include <iostream>

namespace akari::render {

Buffer::Buffer() {
  m_Device         = akari::window::Application::GetDevice();
  m_PhysicalDevice = akari::window::Application::GetPhysicalDevice();
  CreateBuffer(m_Specification.Size);
}

Buffer::Buffer(const Specification& specification)
    : m_Specification(specification) {
  m_Device         = akari::window::Application::GetDevice();
  m_PhysicalDevice = akari::window::Application::GetPhysicalDevice();
}

Buffer::~Buffer() { DestroyBuffer(); }

bool Buffer::Upload(VkCommandBuffer commandBuffer, size_t size, const void* data) {
  if (!size)
    return false;

  if (m_Size >= size) {
    CopyToGPU(commandBuffer, size, data);
    return false;
  }

  VkBuffer      oBuffer                  = m_Buffer;
  VkBuffer      oStagingBuffer           = m_StagingBuffer;
  VmaAllocation oBufferAllocation        = m_BufferAllocation;
  VmaAllocation oStagingBufferAllocation = m_StagingBufferAllocation;

  CreateBuffer(size);

  CopyToGPU(commandBuffer, size, data);

  vmaDestroyBuffer(akari::window::Application::GetVmaAllocator(), oBuffer, oBufferAllocation);
  vmaDestroyBuffer(akari::window::Application::GetVmaAllocator(), oStagingBuffer, oStagingBufferAllocation);

  return true;
}

VkBufferMemoryBarrier2 Buffer::GetBarrier(VkPipelineStageFlags2 dstStageMask,
                                          VkAccessFlags2        dstAccessMask) {
  return VkBufferMemoryBarrier2{
      .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
      .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstStageMask        = dstStageMask,
      .dstAccessMask       = dstAccessMask,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer              = m_Buffer,
      .offset              = 0,
      .size                = VK_WHOLE_SIZE,
  };
}

VkDeviceAddress Buffer::GetDeviceAddress() const {
  VkBufferDeviceAddressInfo addressInfo{
      .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = m_Buffer,
  };

  return vkGetBufferDeviceAddress(m_Device, &addressInfo);
}

void Buffer::CreateBuffer(VkDeviceSize size) {

  m_Size = size;

  // Staging Buffer
  {
    VkBufferCreateInfo bufferInfo{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = m_Size,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    vmaCreateBuffer(akari::window::Application::GetVmaAllocator(), &bufferInfo, &allocInfo, &m_StagingBuffer, &m_StagingBufferAllocation, nullptr);
  }

  // GPU Buffer
  {
    VkBufferCreateInfo bufferInfo{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = m_Size,
        .usage       = m_Specification.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocInfo = {.usage = VMA_MEMORY_USAGE_AUTO};

    vmaCreateBuffer(akari::window::Application::GetVmaAllocator(), &bufferInfo, &allocInfo, &m_Buffer, &m_BufferAllocation, nullptr);
  }
}

void Buffer::DestroyBuffer() {
  vmaDestroyBuffer(akari::window::Application::GetVmaAllocator(), m_Buffer, m_BufferAllocation);
  vmaDestroyBuffer(akari::window::Application::GetVmaAllocator(), m_StagingBuffer, m_StagingBufferAllocation);

  m_Size = 0;
}

void Buffer::CopyToGPU(VkCommandBuffer commandBuffer, size_t size, const void* data) {
  vmaCopyMemoryToAllocation(akari::window::Application::GetVmaAllocator(), data, m_StagingBufferAllocation, 0, size);

  VkBufferCopy copyRegion{
      .srcOffset = 0,
      .dstOffset = 0,
      .size      = size,
  };

  vkCmdCopyBuffer(commandBuffer, m_StagingBuffer, m_Buffer, 1, &copyRegion);
}
} // namespace akari::render
