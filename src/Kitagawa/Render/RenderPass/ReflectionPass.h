#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include "Kitagawa/Render/Buffer.h"
#include "Kitagawa/Render/CameraBuffer.h"
#include "Kitagawa/Render/Pipeline.h"
#include "Kitagawa/Render/RenderPass/Reflection/SSRPass.h"
#include "Kitagawa/World.h"

#include "Application.h"
#include "Image.h"

namespace Kitagawa {
namespace Render {
namespace RenderPass {

class ReflectionPass : public IPipeline {

public:
  enum Type {
    SSR = 0,
  };

private:
  std::unique_ptr<IPipeline> m_Pipeline;

public:
  void Initialize(const std::any &info) override {
    m_Pipeline->Initialize(info);
  }

  void Render(VkCommandBuffer commandBuffer) override {
    m_Pipeline->Render(commandBuffer);
  };

  void CreateBuffers() override { m_Pipeline->CreateBuffers(); };

  void GetDescriptorPoolSize(std::vector<VkDescriptorPoolSize> &pool) override {
    m_Pipeline->GetDescriptorPoolSize(pool);
  };

  void CreateDescriptorSetLayout() override {
    m_Pipeline->CreateDescriptorSetLayout();
  };

  void CreateDescriptorSets(VkDescriptorPool descriptorPool) override {
    m_Pipeline->CreateDescriptorSets(descriptorPool);
  };

  void CreatePipeline() override { m_Pipeline->CreatePipeline(); };

  bool OnResize(uint32_t width, uint32_t height) override {
    return m_Pipeline->OnResize(width, height);
  };

  std::shared_ptr<Akari::Image> GetTexture(Binding binding) override {
    return m_Pipeline->GetTexture(binding);
  }

  void SelectOption(Type type) {
    switch (type) {
    case Type::SSR:
      m_Pipeline = std::make_unique<SSRPass>();
      break;

    default:
      throw std::runtime_error("Invalid reflection pass choice.");
      break;
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
