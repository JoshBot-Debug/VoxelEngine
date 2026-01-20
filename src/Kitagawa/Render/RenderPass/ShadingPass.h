#pragma once

#include <memory>
#include <vector>

#include "Kitagawa/Render/Buffer.h"
#include "Kitagawa/Render/CameraBuffer.h"
#include "Kitagawa/Render/Pipeline.h"
#include "Kitagawa/World.h"

#include "Application.h"
#include "Image.h"

namespace Kitagawa {
namespace Render {
namespace RenderPass {

struct ShadingPassInit {
  World *world = nullptr;

  std::shared_ptr<Akari::Image> directLightTexture = nullptr;
  std::shared_ptr<Akari::Image> shadowTexture = nullptr;
  std::shared_ptr<Akari::Image> ambientOcclusionTexture = nullptr;
};

class ShadingPass : public Pipeline<ShadingPassInit> {

public:
private:
  struct alignas(16) Metadata {
    uint32_t frame;
  };

private:
  VkDevice m_Device;

  VkBuffer m_MetadataBuffer;
  VmaAllocation m_MetadataAllocation;

  std::shared_ptr<Akari::Image> m_Shading =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "ShadingPass::m_Shading",
      });

public:
  ShadingPass();
  ~ShadingPass();

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
    case Binding::T_SHADING:
      return m_Shading;
    default:
      throw std::runtime_error("Invalid binding " + binding);
    }
  }

  uint32_t GetMaxSets() {
    const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();
    // m_Camera(5) + m_MetadataBuffer
    return framesInFlight + 1;
  }
};

} // namespace RenderPass
} // namespace Render
} // namespace Kitagawa
