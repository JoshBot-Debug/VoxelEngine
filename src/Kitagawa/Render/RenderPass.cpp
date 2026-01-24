#include "RenderPass.h"

#include "Application.h"
#include <array>

namespace Kitagawa {
namespace Render {

void RenderPass::CreateRenderPass() {
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
    throw std::runtime_error("failed to create gbuffer framebuffer!");

  vkDestroyFramebuffer(device, previousFramebuffer, nullptr);
}

} // namespace Render
} // namespace Kitagawa
