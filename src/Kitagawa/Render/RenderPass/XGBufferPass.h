#pragma once

#include <memory>

#include "Kitagawa/Render/Buffer.h"
#include "Kitagawa/Render/CameraBuffer.h"
#include "Kitagawa/Render/Pipeline.h"
#include "Kitagawa/Render/RenderPass.h"
#include "Kitagawa/World.h"

#include "Application.h"
#include "Camera/PerspectiveCamera.h"
#include "Image.h"

namespace Kitagawa {
namespace Render {

class XGBufferPass : public RenderPass {

public:
  struct Initializer {
    World*             world        = nullptr;
    Buffer*            vertexBuffer = nullptr;
    CameraBuffer*      cameraBuffer = nullptr;
    PerspectiveCamera* camera       = nullptr;
    uint32_t           width        = 0;
    uint32_t           height       = 0;
  };

private:
  Initializer m_Init;

  VkBuffer      m_DepthBuffer;
  VmaAllocation m_DepthBufferAllocation;
  void*         m_DepthBufferMappedPtr = nullptr;

  std::shared_ptr<Akari::Image> m_Depth =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .Format     = VK_FORMAT_D32_SFLOAT,
          .Usage      = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          .ObjectName = "GBufferPass::m_Depth",
      });

  std::shared_ptr<Akari::Image> m_Normal =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
          .Usage =
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "GBufferPass::m_Normal",
      });

  std::shared_ptr<Akari::Image> m_Material =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R8_UINT,
          .Usage =
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .MagFilter  = VK_FILTER_NEAREST,
          .MinFilter  = VK_FILTER_NEAREST,
          .ObjectName = "GBufferPass::m_Material",
      });

  std::shared_ptr<Akari::Image> m_MotionVector =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R16G16_SFLOAT,
          .Usage =
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "GBufferPass::m_MotionVector",
      });

public:
  void Initialize(const Initializer& init);

  void CreateBuffer() override;

  bool ResizeFramebuffer(uint32_t width, uint32_t height) override;

  std::shared_ptr<Akari::Image> GetTexture(Binding binding) override;
};

} // namespace Render
} // namespace Kitagawa
