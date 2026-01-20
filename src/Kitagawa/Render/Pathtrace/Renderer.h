#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Camera/PerspectiveCamera.h"

#include "Kitagawa/Render/Buffer.h"
#include "Kitagawa/Render/Renderer.h"
#include "Kitagawa/World.h"

#include "Application.h"
#include "Image.h"

using namespace Kitagawa::Render;

namespace Kitagawa {
namespace Pathtrace {

class Renderer : public Kitagawa::Render::Renderer {

private:
  struct UIProperties {
    std::vector<std::shared_ptr<Akari::Image>> displayThumbnails = {};
  };

  struct alignas(16) CamearUBO {
    // Vulkan requires 16 byte alignment, otherwise your data in the shader
    // would be some garbage
    glm::mat4 inverseView;
    glm::mat4 inverseProjection;
  };

  struct alignas(16) PathtraceMetadataUBO {
    uint32_t worldSize;
    uint32_t materialSize;
    uint32_t sampleIndex;
  };

private:
  Buffer m_SVOBuffer;
  Buffer m_MaterialBuffer;
  Buffer m_MaterialLUTBuffer;
  World *m_World = nullptr;
  PerspectiveCamera *m_Camera = nullptr;
  PathtraceMetadataUBO m_MetadataUBO;
  UIProperties m_UIProperties;

  std::shared_ptr<Akari::Image> m_OutputImage =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Width = 800,
          .Height = 600,
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          .ObjectName = "Pathtrace::m_OutputImage",
      });

  VkDevice m_Device = VK_NULL_HANDLE;
  VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

  std::vector<VkDescriptorSetLayout> m_PathtraceDescriptorSetLayouts = {
      VK_NULL_HANDLE, VK_NULL_HANDLE};
  std::vector<std::vector<VkDescriptorSet>> m_PathtraceDescriptorSets =
      std::vector<std::vector<VkDescriptorSet>>(2);
  VkPipelineLayout m_PathtracePipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_PathtracePipeline = VK_NULL_HANDLE;

  std::vector<VkBuffer> m_UniformBuffers;
  std::vector<VmaAllocation> m_UniformBuffersAllocation;
  std::vector<void *> m_UniformBuffersMapped;

  VkBuffer m_PathtraceMetadataBuffer;
  VmaAllocation m_PathtraceMetadataBufferAllocation;

private:
  VkShaderModule CreateShaderModule(const std::string &filename);

  void CreateBuffers();
  void CreateDescripterPool();

  void CreatePathtraceDescripterSetLayout();
  void CreatePathtraceDescriptorSets();
  void CreatePathtracePipline();

  void RecreateDescriptors();

public:
  Renderer();
  ~Renderer();

  void Initialize();

  void Render();

  void OnResize(uint32_t width, uint32_t height);

  void SetCamera(PerspectiveCamera *camera) { m_Camera = camera; };

  void SetWorld(World *world) { m_World = world; };

  void RenderUI();
};

} // namespace Pathtrace
} // namespace Kitagawa