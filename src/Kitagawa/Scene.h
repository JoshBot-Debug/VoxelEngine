#pragma once

#include "Binding.h"
#include "Image.h"
#include "Kitagawa/Render/CameraBuffer.h"
#include "Kitagawa/World.h"

#include "Render/Buffer.h"
#include "Render/Pipeline.h"
#include "Render/RenderPass.h"

class Scene {
private:
  struct InitializeInfo {
    uint32_t width;
    uint32_t height;
  };

private:
  PerspectiveCamera*             m_Camera = nullptr;
  Kitagawa::World*               m_World  = nullptr;
  Kitagawa::Render::CameraBuffer m_CameraBuffer;

  VkDescriptorPool m_DescriptorPool;

  Kitagawa::Render::Buffer m_SVOBuffer;
  Kitagawa::Render::Buffer m_LightBuffer;
  Kitagawa::Render::Buffer m_MaterialBuffer;
  Kitagawa::Render::Buffer m_MaterialLUTBuffer;
  Kitagawa::Render::Buffer m_VertexBuffer{{.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT}};

  uint32_t m_VertexCount = 0;

  Akari::Render::RenderPass m_GBufferPass;
  Akari::Render::Pipeline   m_LightingPipeline;
  Akari::Render::Pipeline   m_ShadingPipeline;
  Akari::Render::Pipeline   m_GeometryPipeline;

  std::shared_ptr<Akari::Image> m_Depth =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .Format     = VK_FORMAT_D32_SFLOAT,
          .Usage      = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          .ObjectName = "m_GBufferPass::m_Depth",
      });

  std::shared_ptr<Akari::Image> m_Normal =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
          .Usage =
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "m_GBufferPass::m_Normal",
      });

  std::shared_ptr<Akari::Image> m_Material =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R8_UINT,
          .Usage =
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .MagFilter  = VK_FILTER_NEAREST,
          .MinFilter  = VK_FILTER_NEAREST,
          .ObjectName = "m_GBufferPass::m_Material",
      });

  std::shared_ptr<Akari::Image> m_MotionVector =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R16G16_SFLOAT,
          .Usage =
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "m_GBufferPass::m_MotionVector",
      });

  std::shared_ptr<Akari::Image> m_DirectLight =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format     = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage      = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "m_LightingPipeline::m_DirectLight",
      });

  std::shared_ptr<Akari::Image> m_OutputImage =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format     = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage      = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "m_ShadingPipeline::m_OutputImage",
      });

  std::vector<std::shared_ptr<Akari::Image>> m_DisplayImages = {};

public:
  Scene();
  ~Scene();

  void Initialize(const InitializeInfo& init);

  void Render();

  void RenderUI();

  void CreateDescriptorSets();

  void OnResize(uint32_t width, uint32_t height);

  void SetCamera(PerspectiveCamera* camera) { m_Camera = camera; };

  void SetWorld(Kitagawa::World* world) { m_World = world; };
};