#pragma once

#include <memory>
#include <vector>

#include "Kitagawa/Render/Binding.h"
#include "Kitagawa/Render/Buffer.h"
#include "Kitagawa/Render/CameraBuffer.h"
#include "Kitagawa/Render/Pipeline.h"
#include "Kitagawa/Render/RenderPass/BilateralFilterPass.h"
#include "Kitagawa/World.h"

#include "Camera/PerspectiveCamera.h"

#include "Application.h"
#include "Image.h"

namespace Kitagawa {
namespace Render {
namespace RenderPass {

struct SSAOPassInit {
  World *world = nullptr;
  Buffer *svoBuffer = nullptr;
  Buffer *materialBuffer = nullptr;
  Buffer *materialLUTBuffer = nullptr;
  CameraBuffer *cameraBuffer = nullptr;
  PerspectiveCamera *camera = nullptr;
  std::shared_ptr<Akari::Image> depthTexture = nullptr;
  std::shared_ptr<Akari::Image> normalTexture = nullptr;
  std::shared_ptr<Akari::Image> motionVectorTexture = nullptr;
};

class SSAOPass : public Pipeline<SSAOPassInit> {

public:
private:
  struct alignas(16) Metadata {
    uint32_t frame = 0;
    uint32_t worldSize = 0;
    uint32_t kernelSize = 0;
    uint32_t sampleIndex = 0;
    glm::vec4 kernal[64] = {};
  };

private:
  VkDevice m_Device;

  VkBuffer m_MetadataBuffer;
  VmaAllocation m_MetadataAllocation;

  Metadata m_Metadata;

  std::shared_ptr<Akari::Image> m_AmbientOcclusionRaw =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R8_UNORM,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          .ObjectName = "SSAOPass::m_AmbientOcclusionRaw",
      });

  std::shared_ptr<Akari::Image> m_Noise =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Width = 4,
          .Height = 4,
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          .MagFilter = VK_FILTER_NEAREST,
          .MinFilter = VK_FILTER_NEAREST,
          .AddressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .AddressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .AddressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .ObjectName = "SSAOPass::m_Noise",
      });

  BilateralFilterPass m_BilateralFilterPass;

public:
  SSAOPass();
  ~SSAOPass();

  void Initialize(const std::any &info) override;

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
   * Create the pipeline
   */
  void CreatePipeline() override;

  /**
   * When the screen resizes
   */
  bool OnResize(uint32_t width, uint32_t height) override;

  std::shared_ptr<Akari::Image> GetTexture(Binding binding) override {
    switch (binding) {
    case Binding::T_AMBIENT_OCCLUSION:
      return m_BilateralFilterPass.GetTexture(T_BILATERAL_BLUR_OUTPUT);
    default:
      throw std::runtime_error("Invalid binding " + binding);
    }
  }

  uint32_t GetMaxSets() { return 0; }
};

} // namespace RenderPass
} // namespace Render
} // namespace Kitagawa
