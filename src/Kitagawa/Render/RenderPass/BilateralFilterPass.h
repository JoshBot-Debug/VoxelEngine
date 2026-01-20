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

struct BilateralFilterPassInit {
  float radius = 4.0f;
  float sigmaD = 2.0f;
  float sigmaH = 128.0f;
  std::shared_ptr<Akari::Image> inputTexture = nullptr;
  std::shared_ptr<Akari::Image> depthTexture = nullptr;
  std::shared_ptr<Akari::Image> normalTexture = nullptr;
  Akari::Image::Specification outputSpecification;
};

class BilateralFilterPass : public Pipeline<BilateralFilterPassInit> {

public:
private:
  struct alignas(16) Metadata {
    float radius = 4.0f;
    float sigmaD = 2.0f;
    float sigmaH = 128.0f;
    uint32_t padding;
  };

private:
  VkDevice m_Device;

  VkBuffer m_MetadataBuffer;
  VmaAllocation m_MetadataAllocation;

  std::shared_ptr<Akari::Image> m_Output = std::make_shared<Akari::Image>();

public:
  BilateralFilterPass();
  ~BilateralFilterPass();

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
    case Binding::T_BILATERAL_BLUR_OUTPUT:
      return m_Output;
    default:
      throw std::runtime_error("Invalid binding " + binding);
    }
  }

  uint32_t GetMaxSets() { return 0; }
};

} // namespace RenderPass
} // namespace Render
} // namespace Kitagawa
