#include "Image.h"
#include "window/Application.h"

#include "imgui_impl_vulkan.h"

namespace akari::window {

Image::Image(const Specification& specification)
    : m_Specification(specification) {
  Initialize(specification);
}

Image::~Image() { Release(); }

void Image::Initialize(const Specification& specification) {
  m_Specification = specification;

  VkDevice     device    = akari::window::Application::GetDevice();
  VmaAllocator allocator = akari::window::Application::GetVmaAllocator();
  VkResult     err;

  // Create the Image
  {
    VkImageCreateInfo info {};

    info.sType     = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags     = specification.Flags;
    info.imageType = specification.ImageType;
    info.format    = specification.Format;

    VkExtent3D extent {};
    extent.width  = specification.Width;
    extent.height = specification.Height;
    extent.depth  = specification.Depth;
    info.extent   = extent;

    info.mipLevels     = specification.MipLevels;
    info.arrayLayers   = specification.ArrayLayers;
    info.samples       = VK_SAMPLE_COUNT_1_BIT;
    info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    info.usage         = specification.Usage;
    info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    err             = vmaCreateImage(allocator, &info, &allocInfo, &m_Image, &m_ImageAllocation, nullptr);
    CheckVkResult(err);
  }

  // Create the Image View:
  {
    VkImageViewCreateInfo info {};

    info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image    = m_Image;
    info.viewType = specification.ViewType;
    info.format   = specification.Format;

    VkImageSubresourceRange subresourceRange {};
    subresourceRange.aspectMask     = specification.AspectMask,
    subresourceRange.baseMipLevel   = 0,
    subresourceRange.levelCount     = specification.MipLevels,
    subresourceRange.baseArrayLayer = 0,
    subresourceRange.layerCount     = specification.ArrayLayers,
    info.subresourceRange           = subresourceRange;

    err = vkCreateImageView(device, &info, nullptr, &m_ImageView);
    CheckVkResult(err);
  }

  // Create the Image Sampler
  {
    VkSamplerCreateInfo info {};
    info.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter        = specification.MagFilter;
    info.minFilter        = specification.MinFilter;
    info.mipmapMode       = specification.MipmapMode;
    info.addressModeU     = specification.AddressModeU;
    info.addressModeV     = specification.AddressModeV;
    info.addressModeW     = specification.AddressModeW;
    info.anisotropyEnable = specification.AnisotropyEnable;
    info.maxAnisotropy    = 1.0f;
    info.minLod           = specification.MinLOD;
    info.maxLod           = specification.MaxLOD;
    VkResult err          = vkCreateSampler(device, &info, nullptr, &m_Sampler);
    CheckVkResult(err);
  }

  if (specification.ObjectName) {
    SET_DEBUG_OBJECT_NAME(
        device,
        VK_OBJECT_TYPE_IMAGE,
        reinterpret_cast<uint64_t>(m_Image),
        (specification.ObjectName + std::string(" m_Image")).c_str());

    SET_DEBUG_OBJECT_NAME(
        device,
        VK_OBJECT_TYPE_IMAGE_VIEW,
        reinterpret_cast<uint64_t>(m_ImageView),
        (specification.ObjectName + std::string(" m_ImageView")).c_str());

    SET_DEBUG_OBJECT_NAME(
        device,
        VK_OBJECT_TYPE_SAMPLER,
        reinterpret_cast<uint64_t>(m_Sampler),
        (specification.ObjectName + std::string(" m_Sampler")).c_str());
  }
}

void Image::Release() {
  VkDevice device = akari::window::Application::GetDevice();

  akari::window::Application::SubmitResourceFree(
      [sampler = m_Sampler, imageView = m_ImageView, image = m_Image, stagingBuffer = m_StagingBuffer, imageAllocation = m_ImageAllocation, stagingBufferAllocation = m_StagingBufferAllocation]() {
        VmaAllocator allocator = akari::window::Application::GetVmaAllocator();
        VkDevice     device    = akari::window::Application::GetDevice();

        vkDestroySampler(device, sampler, nullptr);
        vkDestroyImageView(device, imageView, nullptr);

        vmaDestroyImage(allocator, image, imageAllocation);
        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
      });

  m_Image                   = VK_NULL_HANDLE;
  m_Sampler                 = VK_NULL_HANDLE;
  m_ImageView               = VK_NULL_HANDLE;
  m_DescriptorSet           = VK_NULL_HANDLE;
  m_StagingBuffer           = VK_NULL_HANDLE;
  m_ImageAllocation         = VK_NULL_HANDLE;
  m_StagingBufferAllocation = VK_NULL_HANDLE;
}

void Image::SetData(const void* data, uint32_t mipLevel, uint32_t currentLayer) {
  VmaAllocator allocator = akari::window::Application::GetVmaAllocator();

  uint32_t mipWidth  = std::max(1u, m_Specification.Width >> mipLevel);
  uint32_t mipHeight = std::max(1u, m_Specification.Height >> mipLevel);
  uint32_t mipDepth  = std::max(1u, m_Specification.Depth >> mipLevel);

  size_t mipSize = ImageSizeBytes(mipWidth, mipHeight, mipDepth, m_Specification.Format);

  // Create the Staging Buffer
  if (!m_StagingBuffer) {
    size_t totalSize = 0;

    for (uint32_t l = 0; l < m_Specification.ArrayLayers; ++l)
      for (uint32_t i = 0; i < m_Specification.MipLevels; ++i) {
        uint32_t w = std::max(1u, m_Specification.Width >> i);
        uint32_t h = std::max(1u, m_Specification.Height >> i);
        uint32_t d = std::max(1u, m_Specification.Depth >> i);
        totalSize += ImageSizeBytes(w, h, d, m_Specification.Format);
      }

    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = totalSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo {};
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    CheckVkResult(vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &m_StagingBuffer, &m_StagingBufferAllocation, nullptr));
  }

  size_t offset = 0;

  // Skip previous layers
  for (uint32_t l = 0; l < currentLayer; ++l) {
    for (uint32_t m = 0; m < m_Specification.MipLevels; ++m) {
      uint32_t w = std::max(1u, m_Specification.Width >> m);
      uint32_t h = std::max(1u, m_Specification.Height >> m);
      offset += ImageSizeBytes(w, h, 1, m_Specification.Format);
    }
  }

  // Skip previous mips of this layer
  for (uint32_t m = 0; m < mipLevel; ++m) {
    uint32_t w = std::max(1u, m_Specification.Width >> m);
    uint32_t h = std::max(1u, m_Specification.Height >> m);
    offset += ImageSizeBytes(w, h, 1, m_Specification.Format);
  }

  for (uint32_t i = 0; i < mipLevel; ++i) {
    uint32_t w = std::max(1u, m_Specification.Width >> i);
    uint32_t h = std::max(1u, m_Specification.Height >> i);
    uint32_t d = std::max(1u, m_Specification.Depth >> i);
    offset += ImageSizeBytes(w, h, d, m_Specification.Format);
  }

  CheckVkResult(vmaCopyMemoryToAllocation(
      allocator,
      data,
      m_StagingBufferAllocation,
      offset,
      mipSize));
}

void Image::CopyToImage(VkCommandBuffer commandBuffer) {

  Transition(commandBuffer, m_CurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_NONE, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT, 0, m_Specification.MipLevels, 0, m_Specification.ArrayLayers);

  size_t offset = 0;

  std::vector<VkBufferImageCopy> regions {};

  for (uint32_t layer = 0; layer < m_Specification.ArrayLayers; ++layer) {
    for (uint32_t mip = 0; mip < m_Specification.MipLevels; ++mip) {

      uint32_t mipWidth  = std::max(1u, m_Specification.Width >> mip);
      uint32_t mipHeight = std::max(1u, m_Specification.Height >> mip);
      uint32_t mipDepth  = std::max(1u, m_Specification.Depth >> mip);

      regions.emplace_back(VkBufferImageCopy {
          .bufferOffset = offset,
          .imageSubresource =
              {
                  .aspectMask     = m_Specification.AspectMask,
                  .mipLevel       = mip,
                  .baseArrayLayer = layer,
                  .layerCount     = 1,
              },
          .imageOffset = {0, 0, 0},
          .imageExtent =
              {
                  .width  = mipWidth,
                  .height = mipHeight,
                  .depth  = mipDepth,
              },
      });

      offset += ImageSizeBytes(mipWidth, mipHeight, mipDepth, m_Specification.Format);
    }
  }

  vkCmdCopyBufferToImage(commandBuffer, m_StagingBuffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(regions.size()), regions.data());
}

void Image::Clear(VkCommandBuffer commandBuffer, float r, float g, float b, float a) {
  VkDevice device = akari::window::Application::GetDevice();

  Transition(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkImageSubresourceRange range {};
  range.aspectMask     = m_Specification.AspectMask;
  range.baseMipLevel   = 0;
  range.levelCount     = m_Specification.MipLevels;
  range.baseArrayLayer = 0;
  range.layerCount     = 1;

  VkClearColorValue clearColor {{r, g, b, a}};

  vkCmdClearColorImage(commandBuffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
}

void Image::Transition(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, uint32_t baseMipLevel, uint32_t levelCount, uint32_t baseArrayLayer, uint32_t layerCount) {

  VkImageMemoryBarrier2 barrier {};

  barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask        = srcStageMask;
  barrier.srcAccessMask       = srcAccessMask;
  barrier.dstStageMask        = dstStageMask;
  barrier.dstAccessMask       = dstAccessMask;
  barrier.oldLayout           = oldLayout;
  barrier.newLayout           = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image               = m_Image;

  VkImageSubresourceRange subresourceRange {};
  subresourceRange.aspectMask     = m_Specification.AspectMask;
  subresourceRange.baseMipLevel   = baseMipLevel;
  subresourceRange.levelCount     = levelCount;
  subresourceRange.baseArrayLayer = baseArrayLayer;
  subresourceRange.layerCount     = layerCount;
  barrier.subresourceRange        = subresourceRange;

  VkDependencyInfo depInfo {};
  depInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers    = &barrier;

  vkCmdPipelineBarrier2(commandBuffer, &depInfo);

  m_CurrentLayout      = barrier.newLayout;
  m_CurrentStageMask   = barrier.dstStageMask;
  m_CurrentAccesssMask = barrier.dstAccessMask;
}

void Image::Transition(VkCommandBuffer commandBuffer, VkImageLayout newLayout, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 dstStageMask, uint32_t baseMipLevel, uint32_t levelCount, uint32_t baseArrayLayer, uint32_t layerCount) {

  VkImageMemoryBarrier2 barrier {};
  barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask        = m_CurrentStageMask;
  barrier.srcAccessMask       = m_CurrentAccesssMask;
  barrier.dstStageMask        = dstStageMask;
  barrier.dstAccessMask       = dstAccessMask;
  barrier.oldLayout           = m_CurrentLayout;
  barrier.newLayout           = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image               = m_Image;

  VkImageSubresourceRange subresourceRange {};
  subresourceRange.aspectMask     = m_Specification.AspectMask;
  subresourceRange.baseMipLevel   = baseMipLevel;
  subresourceRange.levelCount     = levelCount;
  subresourceRange.baseArrayLayer = baseArrayLayer;
  subresourceRange.layerCount     = layerCount;
  barrier.subresourceRange        = subresourceRange;

  VkDependencyInfo depInfo {};
  depInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers    = &barrier;

  vkCmdPipelineBarrier2(commandBuffer, &depInfo);

  m_CurrentLayout      = barrier.newLayout;
  m_CurrentStageMask   = barrier.dstStageMask;
  m_CurrentAccesssMask = barrier.dstAccessMask;
}

bool Image::Resize(uint32_t width, uint32_t height) {
  if (m_Image && m_Specification.Width == width &&
      m_Specification.Height == height)
    return false;

  m_Specification.Width  = width;
  m_Specification.Height = height;

  Release();
  Initialize(m_Specification);

  return true;
}

void Image::BindImGuiDescriptor(VkImageLayout layout) {
  m_DescriptorSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
      m_Sampler,
      m_ImageView,
      layout);
}

void Image::SetCurrentState(VkImageLayout         layout,
                            VkPipelineStageFlags2 stageMask,
                            VkAccessFlags2        accessMask) {
  m_CurrentLayout      = layout;
  m_CurrentStageMask   = stageMask;
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

bool Image::IsBlockCompressed(VkFormat format) {
  switch (format) {
  case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
  case VK_FORMAT_BC6H_UFLOAT_BLOCK:
  case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    return true;
  default:
    return false;
  }
}

uint32_t Image::BlockSizeBytes(VkFormat format) {
  switch (format) {
  case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    return 8;

  case VK_FORMAT_BC6H_UFLOAT_BLOCK:
  case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    return 16;

  default:
    throw std::runtime_error("Not a block-compressed format");
  }
}

uint32_t Image::ImageSizeBytes(uint32_t width, uint32_t height, uint32_t depth, VkFormat format) {
  if (!IsBlockCompressed(format))
    return width * height * depth * BytesPerPixel(format);

  uint32_t blockWidth  = (width + 3) / 4;
  uint32_t blockHeight = (height + 3) / 4;

  return blockWidth * blockHeight * depth * BlockSizeBytes(format);
}

} // namespace akari::window