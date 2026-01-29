#include "Image.h"
#include "Application.h"

#include "imgui_impl_vulkan.h"

namespace Akari {

Image::Image(const Specification &specification)
    : m_Specification(specification) {
  Initialize(specification);
}

Image::~Image() { Release(); }

void Image::Initialize(const Specification &specification) {
  m_Specification = specification;

  VkDevice device = Akari::Application::GetDevice();
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();
  VkResult err;

  // Create the Image
  {
    VkImageCreateInfo info = {
        .sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = specification.Flags,
        .imageType = specification.ImageType,
        .format = specification.Format,
        .extent =
            {
                .width = specification.Width,
                .height = specification.Height,
                .depth = specification.Depth,
            },
        .mipLevels = specification.MipLevels,
        .arrayLayers = specification.ArrayLayers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = specification.Usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo = {.usage = VMA_MEMORY_USAGE_AUTO};
    err = vmaCreateImage(allocator, &info, &allocInfo, &m_Image,
                         &m_ImageAllocation, nullptr);
    CheckVkResult(err);
  }

  // Create the Image View:
  {
    VkImageViewCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_Image,
        .viewType = specification.ViewType,
        .format = specification.Format,
        .subresourceRange =
            {
                .aspectMask = specification.AspectMask,
                .baseMipLevel = 0,
                .levelCount = specification.MipLevels,
                .baseArrayLayer = 0,
                .layerCount = specification.ArrayLayers,
            },
    };

    err = vkCreateImageView(device, &info, nullptr, &m_ImageView);
    CheckVkResult(err);
  }

  // Create the Image Sampler
  {
    VkSamplerCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = specification.MagFilter,
        .minFilter = specification.MinFilter,
        .mipmapMode = specification.MipmapMode,
        .addressModeU = specification.AddressModeU,
        .addressModeV = specification.AddressModeV,
        .addressModeW = specification.AddressModeW,
        .anisotropyEnable = specification.AnisotropyEnable,
        .maxAnisotropy = 1.0f,
        .minLod = specification.MinLOD,
        .maxLod = specification.MaxLOD,
    };
    VkResult err = vkCreateSampler(device, &info, nullptr, &m_Sampler);
    CheckVkResult(err);
  }

  if (specification.ObjectName) {
    SET_DEBUG_OBJECT_NAME(
        device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(m_Image),
        (specification.ObjectName + std::string(" m_Image")).c_str());

    SET_DEBUG_OBJECT_NAME(
        device, VK_OBJECT_TYPE_IMAGE_VIEW,
        reinterpret_cast<uint64_t>(m_ImageView),
        (specification.ObjectName + std::string(" m_ImageView")).c_str());

    SET_DEBUG_OBJECT_NAME(
        device, VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<uint64_t>(m_Sampler),
        (specification.ObjectName + std::string(" m_Sampler")).c_str());
  }
}

void Image::Release() {
  VkDevice device = Akari::Application::GetDevice();

  Akari::Application::SubmitResourceFree(
      [sampler = m_Sampler, imageView = m_ImageView, image = m_Image,
       stagingBuffer = m_StagingBuffer, imageAllocation = m_ImageAllocation,
       stagingBufferAllocation = m_StagingBufferAllocation]() {
        VmaAllocator allocator = Akari::Application::GetVmaAllocator();
        VkDevice device = Akari::Application::GetDevice();

        vkDestroySampler(device, sampler, nullptr);
        vkDestroyImageView(device, imageView, nullptr);

        vmaDestroyImage(allocator, image, imageAllocation);
        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
      });

  m_Image = VK_NULL_HANDLE;
  m_Sampler = VK_NULL_HANDLE;
  m_ImageView = VK_NULL_HANDLE;
  m_DescriptorSet = VK_NULL_HANDLE;
  m_StagingBuffer = VK_NULL_HANDLE;
  m_ImageAllocation = VK_NULL_HANDLE;
  m_StagingBufferAllocation = VK_NULL_HANDLE;
}

void Image::SetData(const void *data, uint32_t mipLevel) {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();

  uint32_t mipWidth = std::max(1u, m_Specification.Width >> mipLevel);
  uint32_t mipHeight = std::max(1u, m_Specification.Height >> mipLevel);
  uint32_t mipDepth = std::max(1u, m_Specification.Depth >> mipLevel);

  size_t mipSize =
      mipWidth * mipHeight * mipDepth * BytesPerPixel(m_Specification.Format);

  // Create the Staging Buffer
  if (!m_StagingBuffer) {
    size_t totalSize = 0;

    for (uint32_t i = 0; i < m_Specification.MipLevels; ++i) {
      uint32_t w = std::max(1u, m_Specification.Width >> i);
      uint32_t h = std::max(1u, m_Specification.Height >> i);
      uint32_t d = std::max(1u, m_Specification.Depth >> i);
      totalSize += w * h * d * BytesPerPixel(m_Specification.Format);
    }

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = totalSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    CheckVkResult(vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo,
                                  &m_StagingBuffer, &m_StagingBufferAllocation,
                                  nullptr));
  }

  size_t mipOffset = 0;

  for (uint32_t i = 0; i < mipLevel; ++i) {
    uint32_t w = std::max(1u, m_Specification.Width >> i);
    uint32_t h = std::max(1u, m_Specification.Height >> i);
    uint32_t d = std::max(1u, m_Specification.Depth >> i);
    mipOffset += w * h * d * BytesPerPixel(m_Specification.Format);
  }

  CheckVkResult(vmaCopyMemoryToAllocation(
      allocator, data, m_StagingBufferAllocation, mipOffset, mipSize));
}

void Image::CopyToImage(VkCommandBuffer commandBuffer) {

  Transition(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
             VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT);

  size_t offset = 0;

  std::vector<VkBufferImageCopy> regions = {};

  for (uint32_t mip = 0; mip < m_Specification.MipLevels; ++mip) {

    uint32_t mipWidth = std::max(1u, m_Specification.Width >> mip);
    uint32_t mipHeight = std::max(1u, m_Specification.Height >> mip);
    uint32_t mipDepth = std::max(1u, m_Specification.Depth >> mip);

    regions.emplace_back(VkBufferImageCopy{
        .bufferOffset = offset,
        .imageSubresource =
            {
                .aspectMask = m_Specification.AspectMask,
                .mipLevel = mip,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = {0, 0, 0},
        .imageExtent =
            {
                .width = mipWidth,
                .height = mipHeight,
                .depth = mipDepth,
            },
    });

    offset +=
        mipWidth * mipHeight * mipDepth * BytesPerPixel(m_Specification.Format);
  }

  vkCmdCopyBufferToImage(commandBuffer, m_StagingBuffer, m_Image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(regions.size()), regions.data());
}

void Image::Clear(VkCommandBuffer commandBuffer, float r, float g, float b,
                  float a) {
  VkDevice device = Akari::Application::GetDevice();

  Transition(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
             VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkImageSubresourceRange range = {
      .aspectMask = m_Specification.AspectMask,
      .baseMipLevel = 0,
      .levelCount = m_Specification.MipLevels,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };

  VkClearColorValue clearColor = {{r, g, b, a}};

  vkCmdClearColorImage(commandBuffer, m_Image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1,
                       &range);
}

void Image::Transition(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout,
                       VkAccessFlags2 srcAccessMask,
                       VkAccessFlags2 dstAccessMask,
                       VkPipelineStageFlags2 srcStageMask,
                       VkPipelineStageFlags2 dstStageMask,
                       uint32_t baseMipLevel, uint32_t levelCount,
                       uint32_t baseArrayLayer, uint32_t layerCount) {

  VkImageMemoryBarrier2 barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = m_Image,
      .subresourceRange =
          {
              .aspectMask = m_Specification.AspectMask,
              .baseMipLevel = baseMipLevel,
              .levelCount = levelCount,
              .baseArrayLayer = baseArrayLayer,
              .layerCount = layerCount,
          },
  };

  VkDependencyInfo depInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(commandBuffer, &depInfo);

  m_CurrentLayout = barrier.newLayout;
  m_CurrentStageMask = barrier.dstStageMask;
  m_CurrentAccesssMask = barrier.dstAccessMask;
}

void Image::Transition(VkCommandBuffer commandBuffer, VkImageLayout newLayout,
                       VkAccessFlags2 dstAccessMask,
                       VkPipelineStageFlags2 dstStageMask,
                       uint32_t baseMipLevel, uint32_t levelCount,
                       uint32_t baseArrayLayer, uint32_t layerCount) {

  VkImageMemoryBarrier2 barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = m_CurrentStageMask,
      .srcAccessMask = m_CurrentAccesssMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = m_CurrentLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = m_Image,
      .subresourceRange =
          {
              .aspectMask = m_Specification.AspectMask,
              .baseMipLevel = baseMipLevel,
              .levelCount = levelCount,
              .baseArrayLayer = baseArrayLayer,
              .layerCount = layerCount,
          },
  };

  VkDependencyInfo depInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(commandBuffer, &depInfo);

  m_CurrentLayout = barrier.newLayout;
  m_CurrentStageMask = barrier.dstStageMask;
  m_CurrentAccesssMask = barrier.dstAccessMask;
}

bool Image::Resize(uint32_t width, uint32_t height) {
  if (m_Image && m_Specification.Width == width &&
      m_Specification.Height == height)
    return false;

  m_Specification.Width = width;
  m_Specification.Height = height;

  Release();
  Initialize(m_Specification);

  return true;
}

void Image::BindImGuiDescriptor(VkImageLayout layout) {
  m_DescriptorSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
      m_Sampler, m_ImageView, layout);
}

void Image::SetCurrentState(VkImageLayout layout,
                            VkPipelineStageFlags2 stageMask,
                            VkAccessFlags2 accessMask) {
  m_CurrentLayout = layout;
  m_CurrentStageMask = stageMask;
  m_CurrentAccesssMask = accessMask;
}

uint32_t Image::BytesPerPixel(VkFormat format) {
  switch (format) {
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8_SRGB:
    return 1;

  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8_SRGB:
    return 2;

  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_R8G8B8_SRGB:
    return 3;

  case VK_FORMAT_B8G8R8_UNORM:
  case VK_FORMAT_B8G8R8_SNORM:
  case VK_FORMAT_B8G8R8_UINT:
  case VK_FORMAT_B8G8R8_SINT:
  case VK_FORMAT_B8G8R8_SRGB:
    return 3;

  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_SRGB:
    return 4;

  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16_SFLOAT:
    return 2;

  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16_SFLOAT:
    return 4;

  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16_SFLOAT:
    return 6;

  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return 8;

  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32_SFLOAT:
    return 4;

  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32_SFLOAT:
    return 8;

  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32B32_SFLOAT:
    return 12;

  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return 16;

  case VK_FORMAT_D16_UNORM:
    return 2;

  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return 4;

  case VK_FORMAT_D32_SFLOAT:
    return 4;

  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return 8;

  default:
    throw std::runtime_error("Unsupported format");
  }
}

} // namespace Akari