#include "Buffer.h"

#include "window/Application.h"

using namespace akari::window;

namespace akari::render {

void Buffer::CreateDeviceBuffer(uint64_t size) {
  auto* allocator = Application::GetVmaAllocator();

  VkBuffer      previousBuffer     = m_DeviceBuffer;
  VmaAllocation previousAllocation = m_DeviceBufferAllocation;

  uint64_t grow   = size - m_Size;
  uint64_t blocks = (grow + m_Specification.Size - 1) / m_Specification.Size;
  m_Size += blocks * m_Specification.Size;

  VkBufferCreateInfo bufferInfo {};
  bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size        = m_Size;
  bufferInfo.usage       = m_Specification.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo {};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  if (m_Specification.Pool)
    allocInfo.pool = m_Specification.Pool->GetDevicePool();

  vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_DeviceBuffer, &m_DeviceBufferAllocation, nullptr);

  if (previousBuffer)
    vmaDestroyBuffer(allocator, previousBuffer, previousAllocation);
}

void Buffer::CreateHostBuffer(uint64_t size) {
  auto* allocator = Application::GetVmaAllocator();

  VkBuffer      previousBuffer     = m_HostBuffer;
  VmaAllocation previousAllocation = m_HostBufferAllocation;

  uint64_t grow   = size - m_Size;
  uint64_t blocks = (grow + m_Specification.Size - 1) / m_Specification.Size;
  m_Size += blocks * m_Specification.Size;

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

  vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_HostBuffer, &m_HostBufferAllocation, nullptr);

  if (previousBuffer)
    vmaDestroyBuffer(allocator, previousBuffer, previousAllocation);
}

const Buffer::Block& Buffer::Allocate(uint64_t id, uint64_t bytes, bool& outResized) {
  for (auto& block : m_Blocks)
    if (block.Size >= bytes && block.Free) {
      block.Size = bytes;
      return block;
    }

  Block last;

  if (m_Blocks.size())
    last = m_Blocks.back();
  else
    last = Block {
        .Id     = 0,
        .Size   = 0,
        .Offset = 0,
        .Free   = true,
    };

  uint64_t nextOffset = last.Offset + last.Size;

  if (nextOffset + bytes > m_Size) {
    CreateHostBuffer(nextOffset + bytes);
    CreateDeviceBuffer(nextOffset + bytes);
    outResized = true;
  }

  return m_Blocks.emplace_back(Block {
      .Id     = id,
      .Size   = bytes,
      .Offset = last.Offset + last.Size,
      .Free   = false,
  });
}

void Buffer::HostToDevice(VkCommandBuffer commandBuffer, const Block& block, const void* data) {
  auto* allocator = Application::GetVmaAllocator();

  vmaCopyMemoryToAllocation(allocator, data, m_HostBufferAllocation, block.Offset, block.Size);

  VkBufferCopy copyRegion {};
  copyRegion.srcOffset = block.Offset;
  copyRegion.dstOffset = block.Offset;
  copyRegion.size      = block.Size;

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

void Buffer::CreateBuffer(uint64_t size) {
  CreateHostBuffer(size);
  CreateDeviceBuffer(size);
}

void Buffer::DestroyBuffer() {
  auto* allocator = Application::GetVmaAllocator();

  vmaDestroyBuffer(allocator, m_DeviceBuffer, m_DeviceBufferAllocation);
  vmaDestroyBuffer(allocator, m_HostBuffer, m_HostBufferAllocation);
  m_Size = 0;
  m_Blocks.clear();
}

bool Buffer::Upload(VkCommandBuffer commandBuffer, uint64_t id, size_t size, const void* data) {
  if (!size)
    return false;

  bool resized = false;

  const Block& block = Allocate(id, size, resized);

  HostToDevice(commandBuffer, block, data);

  return resized;
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