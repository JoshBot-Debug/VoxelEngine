#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

namespace akari::window {

class Image {

public:
  struct Specification {
    uint32_t             Width            = 256;
    uint32_t             Height           = 256;
    uint32_t             Depth            = 1;
    uint32_t             MipLevels        = 1;
    float                MinLOD           = 0.0f;
    float                MaxLOD           = 0.0f;
    VkImageType          ImageType        = VkImageType::VK_IMAGE_TYPE_2D;
    VkImageViewType      ViewType         = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
    VkImageAspectFlags   AspectMask       = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
    VkFormat             Format           = (VkFormat)0;
    VkImageUsageFlags    Usage            = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
    VkFilter             MagFilter        = VK_FILTER_LINEAR;
    VkFilter             MinFilter        = VK_FILTER_LINEAR;
    VkSamplerMipmapMode  MipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    VkSamplerAddressMode AddressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerAddressMode AddressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSamplerAddressMode AddressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkBool32             AnisotropyEnable = VK_FALSE;
    VkImageCreateFlags   Flags            = (VkImageCreateFlags)0;
    uint32_t             ArrayLayers      = 1;
    const char*          ObjectName       = nullptr;
  };

private:
  Specification m_Specification;

  VkBuffer m_StagingBuffer = VK_NULL_HANDLE;

  VkImageLayout         m_CurrentLayout      = VK_IMAGE_LAYOUT_UNDEFINED;
  VkPipelineStageFlags2 m_CurrentStageMask   = VK_PIPELINE_STAGE_2_NONE;
  VkAccessFlags2        m_CurrentAccesssMask = VK_ACCESS_2_NONE;

  VmaAllocation m_ImageAllocation         = VK_NULL_HANDLE;
  VmaAllocation m_StagingBufferAllocation = VK_NULL_HANDLE;

public:
  VkImage         m_Image         = VK_NULL_HANDLE;
  VkSampler       m_Sampler       = VK_NULL_HANDLE;
  VkImageView     m_ImageView     = VK_NULL_HANDLE;
  VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

private:
  void            Release();
  static uint32_t BytesPerPixel(VkFormat format);
  bool            IsBlockCompressed(VkFormat format);
  uint32_t        BlockSizeBytes(VkFormat format);
  uint32_t        ImageSizeBytes(uint32_t width, uint32_t height, uint32_t depth, VkFormat format);

public:
  Image() = default;
  Image(const Specification& specification);
  ~Image();

  void Initialize(const Specification& specification);
  void SetData(const void* data, uint32_t mipLevel = 0, uint32_t currentLayer = 0);
  void CopyToImage(VkCommandBuffer commandBuffer);
  void Clear(VkCommandBuffer commandBuffer, float r, float g, float b, float a);
  void Transition(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1);

  void Transition(VkCommandBuffer commandBuffer, VkImageLayout newLayout, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 dstStageMask, uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1);

  bool Resize(uint32_t width, uint32_t height);

  uint32_t             GetWidth() const { return m_Specification.Width; }
  uint32_t             GetHeight() const { return m_Specification.Height; }
  const Specification& GetSpecification() const { return m_Specification; }

  void BindImGuiDescriptor(VkImageLayout layout);

  void SetCurrentState(VkImageLayout layout, VkPipelineStageFlags2 stageMask, VkAccessFlags2 accessMask);
};

} // namespace akari::window