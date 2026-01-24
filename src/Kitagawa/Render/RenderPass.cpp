#include "RenderPass.h"

#include "Application.h"
#include <array>

namespace Kitagawa {
namespace Render {

void RenderPass::CreateRenderPass(const RenderPassCreateInfo& info) {
  VkDevice device = Akari::Application::GetDevice();

  std::vector<VkAttachmentDescription2> attachments;
  std::vector<VkAttachmentReference2>   colorRefs;

  VkAttachmentReference2 depthRef{
      .sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .pNext      = nullptr,
      .attachment = 0,
      .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
  };

  std::sort(info.attachments.begin(), info.attachments.end(), [](AttachmentDescription2& a, AttachmentDescription2& b) {
    return a.depth > b.depth;
  });

  for (uint32_t i = 0; i < static_cast<uint32_t>(info.attachments.size()); i++) {
    auto& attachment = info.attachments[i];

    auto vkAttachment = VkAttachmentDescription2{
        .sType          = attachment.sType,
        .pNext          = attachment.pNext,
        .flags          = attachment.flags,
        .format         = attachment.format,
        .samples        = attachment.samples,
        .loadOp         = attachment.loadOp,
        .storeOp        = attachment.storeOp,
        .stencilLoadOp  = attachment.stencilLoadOp,
        .stencilStoreOp = attachment.stencilStoreOp,
        .initialLayout  = attachment.initialLayout,
        .finalLayout    = attachment.finalLayout,
    };

    if (!attachment.depth) {
      attachments.emplace_back(vkAttachment);
      colorRefs.emplace_back(VkAttachmentReference2{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT});
    } else {
      attachments.emplace(attachments.begin(), vkAttachment);
      depthRef.attachment = i;
    }
  }

  // Subpass
  VkSubpassDescription2 subpass{
      .sType                   = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
      .pNext                   = nullptr,
      .flags                   = 0,
      .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .viewMask                = 0,
      .inputAttachmentCount    = 0,
      .pInputAttachments       = nullptr,
      .colorAttachmentCount    = static_cast<uint32_t>(colorRefs.size()),
      .pColorAttachments       = colorRefs.data(),
      .pResolveAttachments     = nullptr,
      .pDepthStencilAttachment = &depthRef,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments    = nullptr,
  };

  // Dependencies
  std::vector<VkSubpassDependency2> dependencies{
      VkSubpassDependency2{
          .sType        = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
          .pNext        = nullptr,
          .srcSubpass   = VK_SUBPASS_EXTERNAL,
          .dstSubpass   = 0,
          .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                          VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
          .srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
          .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
          .viewOffset      = 0,
      },
      VkSubpassDependency2{
          .sType        = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
          .pNext        = nullptr,
          .srcSubpass   = 0,
          .dstSubpass   = VK_SUBPASS_EXTERNAL,
          .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
          .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dstAccessMask   = VK_ACCESS_2_SHADER_READ_BIT,
          .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
          .viewOffset      = 0,
      },
  };

  // Render pass info
  VkRenderPassCreateInfo2 rpInfo{
      .sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .pNext                   = nullptr,
      .flags                   = 0,
      .attachmentCount         = static_cast<uint32_t>(attachments.size()),
      .pAttachments            = attachments.data(),
      .subpassCount            = 1,
      .pSubpasses              = &subpass,
      .dependencyCount         = static_cast<uint32_t>(dependencies.size()),
      .pDependencies           = dependencies.data(),
      .correlatedViewMaskCount = 0,
      .pCorrelatedViewMasks    = nullptr,
  };

  if (vkCreateRenderPass2(device, &rpInfo, nullptr, &m_RenderPass) !=
      VK_SUCCESS)
    throw std::runtime_error("Failed to create render pass");
}

void RenderPass::CreateFramebuffer(const FramebufferCreateInfo& info) {
  VkDevice device = Akari::Application::GetDevice();

  VkFramebuffer previousFramebuffer = m_Framebuffer;

  VkFramebufferCreateInfo framebufferInfo{
      .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass      = m_RenderPass,
      .attachmentCount = info.attachmentCount,
      .pAttachments    = info.pAttachments,
      .width           = info.width,
      .height          = info.height,
      .layers          = info.layers,
  };

  VkResult result =
      vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffer);

  if (result != VK_SUCCESS)
    throw std::runtime_error("failed to create framebuffer!");

  vkDestroyFramebuffer(device, previousFramebuffer, nullptr);
}

} // namespace Render
} // namespace Kitagawa
