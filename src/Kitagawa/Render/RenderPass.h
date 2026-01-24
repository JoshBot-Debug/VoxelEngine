#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

namespace Kitagawa {
namespace Render {

class RenderPass {
public:
  struct FramebufferCreateInfo {
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t layers = 1;

    const VkImageView* pAttachments    = nullptr;
    uint32_t           attachmentCount = 0;
  };

  struct AttachmentDescription2 {
    const void*                  pNext;
    VkAttachmentDescriptionFlags flags;
    VkFormat                     format;
    VkSampleCountFlagBits        samples;
    VkAttachmentLoadOp           loadOp;
    VkAttachmentStoreOp          storeOp;
    VkAttachmentLoadOp           stencilLoadOp;
    VkAttachmentStoreOp          stencilStoreOp;
    VkImageLayout                initialLayout;
    VkImageLayout                finalLayout;
  };

private:
  VkRenderPass  m_RenderPass  = nullptr;
  VkFramebuffer m_Framebuffer = nullptr;

public:
  virtual void CreateBuffer(){};

  virtual bool OnResizeFramebuffer(uint32_t width, uint32_t height) {
    return false;
  };

  void CreateRenderPass();

  void CreateFramebuffer(const FramebufferCreateInfo& info);
};

} // namespace Render
} // namespace Kitagawa
