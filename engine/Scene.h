#pragma once

#include "Binding.h"
#include "CameraBuffer.h"
#include "UI.h"
#include "World.h"
#include "window/Image.h"

#include "render/Buffer.h"
#include "render/Pipeline.h"
#include "render/RenderPass.h"

class Scene {
private:
  struct InitializeInfo {
    VkPolygonMode PolygonMode;
    uint32_t Width;
    uint32_t Height;
  };

private:
  akari::camera::PerspectiveCamera* m_Camera = nullptr;
  vxen::World*                  m_World  = nullptr;
  vxen::UI*                     m_UI     = nullptr;
  vxen::CameraBuffer            m_CameraBuffer;

  VkDescriptorPool m_DescriptorPool;

  akari::render::Buffer m_SVOBuffer;
  akari::render::Buffer m_LightBuffer;
  akari::render::Buffer m_MaterialBuffer;
  akari::render::Buffer m_MaterialLUTBuffer;
  akari::render::Buffer m_VertexBuffer{{.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT}};
  akari::render::Buffer m_OverlayVertexBuffer{{.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT}};

  uint32_t m_VertexCount        = 0;
  uint32_t m_OverlayVertexCount = 0;

  akari::render::RenderPass m_GBufferPass;
  akari::render::Pipeline   m_GeometryPipeline;

  akari::render::Pipeline m_LightingPipeline;
  akari::render::Pipeline m_ShadingPipeline;

  akari::render::RenderPass m_OverlayPass;
  akari::render::Pipeline   m_OverlayPipeline;

  std::shared_ptr<akari::window::Image> m_Skybox =
      std::make_shared<akari::window::Image>(akari::window::Image::Specification{
          .Width       = 1024,
          .Height      = 1024,
          .ViewType    = VK_IMAGE_VIEW_TYPE_CUBE,
          .Format      = VK_FORMAT_R8G8B8A8_SRGB,
          .Usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .Flags       = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
          .ArrayLayers = 6,
          .ObjectName  = "m_ShadingPass::m_Skybox",
      });

  std::shared_ptr<akari::window::Image> m_Depth =
      std::make_shared<akari::window::Image>(akari::window::Image::Specification{
          .AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .Format     = VK_FORMAT_D32_SFLOAT,
          .Usage      = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "m_GBufferPass::m_Depth",
      });

  std::shared_ptr<akari::window::Image> m_Normal =
      std::make_shared<akari::window::Image>(akari::window::Image::Specification{
          .Format     = VK_FORMAT_R16G16B16A16_SFLOAT,
          .Usage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "m_GBufferPass::m_Normal",
      });

  std::shared_ptr<akari::window::Image> m_Material =
      std::make_shared<akari::window::Image>(akari::window::Image::Specification{
          .Format     = VK_FORMAT_R8_UINT,
          .Usage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .MagFilter  = VK_FILTER_NEAREST,
          .MinFilter  = VK_FILTER_NEAREST,
          .ObjectName = "m_GBufferPass::m_Material",
      });

  std::shared_ptr<akari::window::Image> m_MotionVector =
      std::make_shared<akari::window::Image>(akari::window::Image::Specification{
          .Format     = VK_FORMAT_R16G16_SFLOAT,
          .Usage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "m_GBufferPass::m_MotionVector",
      });

  std::shared_ptr<akari::window::Image> m_DirectLight =
      std::make_shared<akari::window::Image>(akari::window::Image::Specification{
          .Format     = VK_FORMAT_R16G16B16A16_SFLOAT,
          .Usage      = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "m_LightingPipeline::m_DirectLight",
      });

  std::shared_ptr<akari::window::Image> m_OutputImage =
      std::make_shared<akari::window::Image>(akari::window::Image::Specification{
          .Format     = VK_FORMAT_R16G16B16A16_SFLOAT,
          .Usage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "m_ShadingPipeline::m_OutputImage",
      });

  std::vector<std::shared_ptr<akari::window::Image>> m_DisplayImages = {};

public:
  Scene();
  ~Scene();

  void Initialize(const InitializeInfo& init);

  void Render();

  void RenderUI();

  void CreateDescriptorSets();

  void OnResize(uint32_t width, uint32_t height);

  void SetCamera(akari::camera::PerspectiveCamera* camera) { m_Camera = camera; };

  void SetWorld(vxen::World* world) { m_World = world; };

  void SetUI(vxen::UI* ui) { m_UI = ui; };
};