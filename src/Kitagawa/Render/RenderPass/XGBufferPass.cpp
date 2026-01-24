#include "XGBufferPass.h"

namespace Kitagawa {
namespace Render {

void XGBufferPass::Initialize(const Initializer& init) { m_Init = init; }

void XGBufferPass::CreateBuffer() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();

  // Depth buffer
  {
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = sizeof(float),
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocationInfo allocInfo = {};

    vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &m_DepthBuffer,
                    &m_DepthBufferAllocation, &allocInfo);

    m_DepthBufferMappedPtr = allocInfo.pMappedData;
  }
}

bool XGBufferPass::OnResizeFramebuffer(uint32_t width, uint32_t height) {
  if (!m_Depth->Resize(width, height))
    return false;
  m_Normal->Resize(width, height);
  m_Material->Resize(width, height);
  m_MotionVector->Resize(width, height);

  uint32_t attachmentCount = 4;

  VkImageView pAttachments[attachmentCount] = {
      m_Normal->m_ImageView,
      m_Material->m_ImageView,
      m_MotionVector->m_ImageView,
      m_Depth->m_ImageView,
  };

  CreateFramebuffer({
      .width           = m_Depth->GetWidth(),
      .height          = m_Depth->GetHeight(),
      .pAttachments    = pAttachments,
      .attachmentCount = attachmentCount,
  });

  return true;
}

} // namespace Render
} // namespace Kitagawa