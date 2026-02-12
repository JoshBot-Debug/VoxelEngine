#include "RenderPass.h"

#include "window/Application.h"

namespace akari::render {

RenderPass::~RenderPass() {
  VkDevice device = akari::window::Application::GetDevice();

  vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
  vkDestroyRenderPass(device, m_RenderPass, nullptr);
}

void RenderPass::CreateRenderPass(const RenderPassCreateInfo& info) {
  VkDevice device = akari::window::Application::GetDevice();

  auto infoAttachments = info.attachments;

  std::vector<VkAttachmentDescription2> attachments;
  std::vector<VkAttachmentReference2>   colorRefs;

  VkAttachmentReference2 depthRef{
      .sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .pNext      = nullptr,
      .attachment = 0,
      .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
  };

  std::sort(infoAttachments.begin(), infoAttachments.end(), [](const AttachmentDescription2& a, const AttachmentDescription2& b) {
    return a.depth < b.depth;
  });

  for (uint32_t i = 0; i < static_cast<uint32_t>(infoAttachments.size()); i++) {
    auto& attachment = infoAttachments[i];

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
      attachments.emplace_back(vkAttachment);
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
      .pDepthStencilAttachment = nullptr,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments    = nullptr,
  };

  if (depthRef.attachment > 0)
    subpass.pDepthStencilAttachment = &depthRef;

  // render pass info
  VkRenderPassCreateInfo2 rpInfo{
      .sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .pNext                   = nullptr,
      .flags                   = 0,
      .attachmentCount         = static_cast<uint32_t>(attachments.size()),
      .pAttachments            = attachments.data(),
      .subpassCount            = 1,
      .pSubpasses              = &subpass,
      .correlatedViewMaskCount = 0,
      .pCorrelatedViewMasks    = nullptr,
  };

  if (vkCreateRenderPass2(device, &rpInfo, nullptr, &m_RenderPass) !=
      VK_SUCCESS)
    throw std::runtime_error("Failed to create render pass");
}

void RenderPass::CreateFramebuffer(const FramebufferCreateInfo& info) {
  VkDevice device = akari::window::Application::GetDevice();

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

  m_FramebufferCreateInfo = info;
}

void RenderPass::BeginRenderPass(const BeginRenderPassInfo& info) {
  VkViewport viewport{
      .x        = 0.0f,
      .y        = 0.0f,
      .width    = (float)m_FramebufferCreateInfo.width,
      .height   = (float)m_FramebufferCreateInfo.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  VkRect2D renderArea = {
      .offset = {0, 0},
      .extent =
          {
              .width  = m_FramebufferCreateInfo.width,
              .height = m_FramebufferCreateInfo.height,
          },
  };

  vkCmdSetViewport(info.commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(info.commandBuffer, 0, 1, &renderArea);

  VkRenderPassBeginInfo renderPassBeginInfo{
      .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass      = m_RenderPass,
      .framebuffer     = m_Framebuffer,
      .renderArea      = renderArea,
      .clearValueCount = static_cast<uint32_t>(info.clearColor.size()),
      .pClearValues    = info.clearColor.data(),
  };

  vkCmdBeginRenderPass(info.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

} // namespace akari::render
