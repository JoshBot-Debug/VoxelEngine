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

struct SSRPassInit {
  World *world = nullptr;
  Buffer *materialBuffer = nullptr;
  Buffer *materialLUTBuffer = nullptr;
  CameraBuffer *cameraBuffer = nullptr;
  std::shared_ptr<Akari::Image> depthTexture = nullptr;
  std::shared_ptr<Akari::Image> normalTexture = nullptr;
  std::shared_ptr<Akari::Image> materialTexture = nullptr;
};

class SSRPass : public Pipeline<SSRPassInit> {

public:
private:
  struct alignas(16) Metadata {
    uint32_t frame = 0;
    uint32_t worldSize = 0;
    uint32_t maxSteps = 0;
    float stepSize = 0;
    float epsilon = 0;
  };

private:
  VkDevice m_Device;

  VkBuffer m_MetadataBuffer;
  VmaAllocation m_MetadataAllocation;

  Metadata m_Metadata;

  std::shared_ptr<Akari::Image> m_SSR =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "SSRPass::m_SSR",
      });

public:
  SSRPass();
  ~SSRPass();

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
    case Binding::T_REFLECTION:
      return m_SSR;
    default:
      throw std::runtime_error("Invalid binding " + binding);
    }
  }

  uint32_t GetMaxSets() { return 0; }
};

} // namespace RenderPass
} // namespace Render
} // namespace Kitagawa
