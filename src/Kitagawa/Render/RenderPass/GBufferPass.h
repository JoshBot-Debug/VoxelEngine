#pragma once

#include <memory>
#include <vector>

#include "Kitagawa/Render/Buffer.h"
#include "Kitagawa/Render/CameraBuffer.h"
#include "Kitagawa/Render/Pipeline.h"
#include "Kitagawa/World.h"

#include "Camera/PerspectiveCamera.h"
#include "Application.h"
#include "Image.h"

namespace Kitagawa {
namespace Render {
namespace RenderPass {

struct GBufferPassInit {
  World *world = nullptr;
  Buffer *vertexBuffer = nullptr;
  CameraBuffer *cameraBuffer = nullptr;
  PerspectiveCamera *camera = nullptr;
};

class GBufferPass : public Pipeline<GBufferPassInit> {

public:
  uint32_t m_VertexCount = 0;

private:
  struct alignas(16) Metadata {
    uint32_t frame;
    uint32_t worldSize;
  };

private:
  VkDevice m_Device;

  VkBuffer m_MetadataBuffer;
  VmaAllocation m_MetadataAllocation;

  VkBuffer m_DepthBuffer;
  VmaAllocation m_DepthBufferAllocation;
  void *m_DepthBufferPtr = nullptr;
  VkRect2D m_RenderArea;

  std::shared_ptr<Akari::Image> m_Depth =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .Format = VK_FORMAT_D32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
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
          .MagFilter = VK_FILTER_NEAREST,
          .MinFilter = VK_FILTER_NEAREST,
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
  GBufferPass();
  ~GBufferPass();

  void Render(VkCommandBuffer commandBuffer) override;

  /**
   * First step is to create all the buffers we require
   */
  void CreateBuffers() override;

  /**
   * Second step is to create the descriptor pool size
   */
  void GetDescriptorPoolSize(std::vector<VkDescriptorPoolSize> &pool) override;

  /**
   * Setup the descriptor set layout
   */
  void CreateDescriptorSetLayout() override;

  /**
   * Create descriptor sets
   */
  void CreateDescriptorSets(VkDescriptorPool descriptorPool) override;

  /**
   * Create the render pass
   */
  void CreateRenderPass() override;

  /**
   * Create the framebuffer
   */
  void CreateFramebuffer() override;

  /**
   * Create the pipeline
   */
  void CreatePipeline() override;

  /**
   * When the screen resizes
   */
  bool OnResize(uint32_t width, uint32_t height) override;

  /**
   * Returns the value of the depth buffer at mouse coords
   */
  float *GetDepth();

  std::shared_ptr<Akari::Image> GetTexture(Binding binding) override {
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

  uint32_t GetMaxSets();
};

} // namespace RenderPass
} // namespace Render
} // namespace Kitagawa
