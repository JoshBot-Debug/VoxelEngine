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
namespace Raymarch {

class Renderer : public Kitagawa::Render::Renderer {

private:
  struct UIProperties {
    std::vector<std::shared_ptr<Akari::Image>> displayThumbnails = {};
  };

  struct alignas(16) CamearUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 position;
    glm::mat4 inverseView;
    glm::mat4 inverseProjection;
  };

  struct alignas(16) RaymarchMetadataUBO {
    uint32_t frame;
    uint32_t worldSize;
    uint32_t batchStart;
    uint32_t rays;
    uint32_t probeSize;
    uint32_t probeBatchSize;
    uint32_t iaTilesPerRow;
    uint32_t iaTilesPerCol;
    uint32_t iaWidth;
    uint32_t iaHeight;
    uint32_t daTilesPerRow;
    uint32_t daTilesPerCol;
    uint32_t daWidth;
    uint32_t daHeight;
    uint32_t probeSpacing;
    glm::ivec4 gridResolution;
    glm::ivec4 gridMin;
  };

  struct alignas(16) ShadingMetadataUBO {
    uint32_t frame;
  };

private:
  Buffer m_SVOBuffer;
  Buffer m_LightBuffer;
  Buffer m_MaterialBuffer;
  Buffer m_MaterialLUTBuffer;

  World *m_World = nullptr;
  PerspectiveCamera *m_Camera = nullptr;
  UIProperties m_UIProperties;

  std::shared_ptr<Akari::Image> m_OutputImage =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "Renderer::m_OutputImage",
      });

  std::shared_ptr<Akari::Image> m_AlbedoTexture =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "Renderer::m_AlbedoTexture",
      });

  std::shared_ptr<Akari::Image> m_OcclusionTexture =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "Renderer::m_OcclusionTexture",
      });

  std::shared_ptr<Akari::Image> m_DirectLightTexture =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "Renderer::m_DirectLightTexture",
      });

  std::shared_ptr<Akari::Image> m_IndirectLightTexture =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "Renderer::m_IndirectLightTexture",
      });

  std::shared_ptr<Akari::Image> m_DepthTexture =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "Renderer::m_DepthTexture",
      });

  std::shared_ptr<Akari::Image> m_NormalTexture =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "Renderer::m_NormalTexture",
      });

  std::shared_ptr<Akari::Image> m_DDGIIrradianceAtlas =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .MagFilter = VkFilter::VK_FILTER_NEAREST,
          .MinFilter = VkFilter::VK_FILTER_NEAREST,
          .ObjectName = "Renderer::m_DDGIIrradianceAtlas",
      });

  std::shared_ptr<Akari::Image> m_DDGIGaussianDepthAtlas =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R16G16_UNORM,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .MagFilter = VkFilter::VK_FILTER_NEAREST,
          .MinFilter = VkFilter::VK_FILTER_NEAREST,
          .ObjectName = "Renderer::m_DDGIGaussianDepthAtlas",
      });

  std::shared_ptr<Akari::Image> m_PositionTexture =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "Renderer::m_PositionTexture",
      });

  std::shared_ptr<Akari::Image> m_MaterialTexture =
      std::make_shared<Akari::Image>(Akari::Image::Specification{
          .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "Renderer::m_MaterialTexture",
      });

  VkDevice m_Device = VK_NULL_HANDLE;
  VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_DDGIDescriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_DDGIDescriptorSet = VK_NULL_HANDLE;
  VkPipelineLayout m_DDGIPipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_DDGIPipeline = VK_NULL_HANDLE;

  std::vector<VkDescriptorSetLayout> m_RaymarchDescriptorSetLayouts = {
      VK_NULL_HANDLE, VK_NULL_HANDLE};
  std::vector<std::vector<VkDescriptorSet>> m_RaymarchDescriptorSets =
      std::vector<std::vector<VkDescriptorSet>>(2);
  VkPipelineLayout m_RaymarchPipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_RaymarchPipeline = VK_NULL_HANDLE;

  std::vector<VkDescriptorSetLayout> m_ShadingDescriptorSetLayouts = {
      VK_NULL_HANDLE, VK_NULL_HANDLE};
  std::vector<std::vector<VkDescriptorSet>> m_ShadingDescriptorSets =
      std::vector<std::vector<VkDescriptorSet>>(2);

  VkPipelineLayout m_ShadingPipelineLayout = VK_NULL_HANDLE;
  VkPipeline m_ShadingPipeline = VK_NULL_HANDLE;

  std::vector<VkBuffer> m_UniformBuffers;
  std::vector<VmaAllocation> m_UniformBuffersAllocation;
  std::vector<void *> m_UniformBuffersMapped;

  VkBuffer m_RaymarchMetadataBuffer;
  VmaAllocation m_RaymarchMetadataBufferAllocation;

  VkBuffer m_ShadingMetadataBuffer;
  VmaAllocation m_ShadingMetadataBufferAllocation;

  VkBuffer m_DDGIMetadataBuffer;
  VmaAllocation m_DDGIMetadataBufferAllocation;

  RaymarchMetadataUBO m_RaymarchMetadata;

private:
  VkShaderModule CreateShaderModule(const std::string &filename);

  void CreateBuffers();
  void CreateDescripterPool();

  void CreateDDGIDescripterSetLayout();
  void CreateDDGIDescriptorSets();
  void CreateDDGIPipline();

  void CreateRaymarchDescripterSetLayout();
  void CreateRaymarchDescriptorSets();
  void CreateRaymarchPipline();

  void CreateShadingDescripterSetLayout();
  void CreateShadingDescriptorSets();
  void CreateShadingPipline();

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

} // namespace Raymarch
} // namespace Kitagawa