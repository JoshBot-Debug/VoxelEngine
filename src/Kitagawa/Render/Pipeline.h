#pragma once

#include <any>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "Application.h"
#include "Image.h"
#include "Kitagawa/Render/Binding.h"

namespace Kitagawa {
namespace Render {

class IPipeline {
public:
  virtual ~IPipeline() = default;

  virtual void Initialize(const std::any &info){};
  virtual void Render(VkCommandBuffer commandBuffer) = 0;
  virtual void CreateBuffers() = 0;
  virtual void
  GetDescriptorPoolSize(std::vector<VkDescriptorPoolSize> &pool) = 0;
  virtual void CreateDescriptorSetLayout() = 0;
  virtual void CreateDescriptorSets(VkDescriptorPool descriptorPool) = 0;
  virtual void CreateRenderPass(){};
  virtual void CreateFramebuffer(){};
  virtual void CreatePipeline() = 0;
  virtual bool OnResize(uint32_t width, uint32_t height) = 0;
  virtual std::shared_ptr<Akari::Image> GetTexture(Binding binding) = 0;
  virtual uint32_t GetMaxSets() = 0;
};

template <typename Init> class Pipeline : public IPipeline {
private:
  VkDevice m_Device;

protected:
  Init m_Init;

  VkRenderPass m_RenderPass = VK_NULL_HANDLE;
  VkPipeline m_Pipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
  VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;

  std::vector<std::vector<VkDescriptorSet>> m_DescriptorSets = {};
  std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts = {};

public:
  Pipeline() : m_Device(Akari::Application::GetDevice()) {}

  virtual ~Pipeline() {
    vkDestroyFramebuffer(m_Device, m_Framebuffer, nullptr);
    vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
    for (const auto &layout : m_DescriptorSetLayouts)
      vkDestroyDescriptorSetLayout(m_Device, layout, nullptr);
    vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
    vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
  }

  virtual void Initialize(const std::any &info) {
    m_Init = std::any_cast<Init>(info);
  }

  virtual void Render(VkCommandBuffer commandBuffer) = 0;

  virtual void CreateBuffers() = 0;

  virtual void
  GetDescriptorPoolSize(std::vector<VkDescriptorPoolSize> &pool) = 0;

  virtual void CreateDescriptorSetLayout() = 0;
  virtual void CreateDescriptorSets(VkDescriptorPool descriptorPool) = 0;
  virtual void CreateRenderPass(){};
  virtual void CreateFramebuffer(){};
  virtual void CreatePipeline() = 0;

  virtual bool OnResize(uint32_t width, uint32_t height) = 0;

  VkPipeline GetPipeline() { return m_Pipeline; };
  VkPipelineLayout GetPipelineLayout() { return m_PipelineLayout; };

  virtual std::shared_ptr<Akari::Image> GetTexture(Binding binding) = 0;

  virtual uint32_t GetMaxSets() = 0;

protected:
  VkShaderModule CreateShaderModule(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
      throw std::runtime_error("Failed to open shader file: " + filename);

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t *>(buffer.data()),
    };

    VkShaderModule shaderModule;

    if (LOG_VK_RESULT(vkCreateShaderModule(m_Device, &createInfo, nullptr,
                                           &shaderModule)))
      throw std::runtime_error("Failed to create shader module");

    return shaderModule;
  }
};

} // namespace Render
} // namespace Kitagawa