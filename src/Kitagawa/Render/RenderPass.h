#pragma once

#include <memory>
#include <stdint.h>
#include <vector>
#include <vulkan/vulkan.h>

#include "Image.h"
#include "Binding.h"

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
    VkStructureType              sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    const void*                  pNext = nullptr;
    VkAttachmentDescriptionFlags flags = 0;
    VkFormat                     format;
    VkSampleCountFlagBits        samples        = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp           loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp          storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    VkAttachmentLoadOp           stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp          stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkImageLayout                initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout                finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bool                         depth          = false;
  };

  struct RenderPassCreateInfo {
    std::vector<AttachmentDescription2> attachments;
  };

private:
  VkRenderPass  m_RenderPass  = nullptr;
  VkFramebuffer m_Framebuffer = nullptr;

public:
  virtual void CreateBuffer(){};

  virtual bool ResizeFramebuffer(uint32_t width, uint32_t height) {
    return false;
  };

  virtual std::shared_ptr<Akari::Image> GetTexture(Binding binding) {
    return nullptr;
  };

  void CreateRenderPass(const RenderPassCreateInfo& info);

  void CreateFramebuffer(const FramebufferCreateInfo& info);
};

} // namespace Render
} // namespace Kitagawa
