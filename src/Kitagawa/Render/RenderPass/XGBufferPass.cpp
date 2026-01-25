#include "XGBufferPass.h"

namespace Kitagawa {
namespace Render {

void XGBufferPass::Initialize(const Initializer& init) {
  m_Init = init;
  CreateBuffer();
  ResizeFramebuffer(init.width, init.height);
  CreateRenderPass({.attachments = std::vector<AttachmentDescription2>{
                        {.format = m_Normal->GetSpecification().Format},
                        {.format = m_Material->GetSpecification().Format},
                        {.format = m_MotionVector->GetSpecification().Format},
                        {.format = m_Depth->GetSpecification().Format, .depth = true},
                    }});

  // XPipeline pipeline;
  // pipeline.CreateDescriptorSet({
  //     // .descriptorPool = init.de
  //     .layoutBindings = {
  //         VkDescriptorSetLayoutBinding{
  //             .binding         = Binding::U_CAMERA,
  //             .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
  //             .descriptorCount = 1,
  //             .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
  //         }},
  //     .descriptorSetCount = Akari::Application::GetMaxFramesInFlight(),
  //     .pBufferInfo        = [init = &m_Init](int i) {
  //       return VkDescriptorBufferInfo{.buffer = init->cameraBuffer->GetBuffer(i), .offset = 0, .range = sizeof(CameraBuffer::Camera)};
  //     },
  // });
}

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

    vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &m_DepthBuffer, &m_DepthBufferAllocation, &allocInfo);

    m_DepthBufferMappedPtr = allocInfo.pMappedData;
  }
}

bool XGBufferPass::ResizeFramebuffer(uint32_t width, uint32_t height) {
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

std::shared_ptr<Akari::Image> XGBufferPass::GetTexture(Binding binding) {
  switch (binding) {
  case Binding::T_DEPTH:
    return m_Depth;
  case Binding::T_NORMAL:
    return m_Normal;
  case Binding::T_MATERIAL:
    return m_Material;
  case Binding::T_MOTION_VECTOR:
    return m_MotionVector;
  default:
    throw std::runtime_error("Invalid binding " + binding);
  }
}

} // namespace Render
} // namespace Kitagawa