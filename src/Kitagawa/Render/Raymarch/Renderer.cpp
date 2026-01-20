#include "Renderer.h"

#include "backends/imgui_impl_vulkan.h"

#include "Application.h"
#include "Input/Input.h"
#include "Utility/Utility.h"
#include <algorithm>

#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <iomanip>
#include <iostream>

namespace Kitagawa {
namespace Raymarch {

Renderer::Renderer() {
  m_Device = Akari::Application::GetDevice();

  m_UIProperties.displayThumbnails = {
      m_OutputImage,         m_AlbedoTexture,          m_OcclusionTexture,
      m_DirectLightTexture,  m_IndirectLightTexture,   m_DepthTexture,
      m_NormalTexture,       m_PositionTexture,        m_MaterialTexture,
      m_DDGIIrradianceAtlas, m_DDGIGaussianDepthAtlas,
  };
}

Renderer::~Renderer() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();

  for (size_t i = 0; i < Akari::Application::GetMaxFramesInFlight(); i++) {
    vmaDestroyBuffer(allocator, m_UniformBuffers[i],
                     m_UniformBuffersAllocation[i]);
  }

  for (const auto &layout : m_RaymarchDescriptorSetLayouts)
    vkDestroyDescriptorSetLayout(m_Device, layout, nullptr);
  vkDestroyPipelineLayout(m_Device, m_RaymarchPipelineLayout, nullptr);
  vkDestroyPipeline(m_Device, m_RaymarchPipeline, nullptr);

  for (const auto &layout : m_ShadingDescriptorSetLayouts)
    vkDestroyDescriptorSetLayout(m_Device, layout, nullptr);

  vkDestroyPipelineLayout(m_Device, m_ShadingPipelineLayout, nullptr);
  vkDestroyPipeline(m_Device, m_ShadingPipeline, nullptr);

  vkDestroyDescriptorSetLayout(m_Device, m_DDGIDescriptorSetLayout, nullptr);
  vkDestroyPipelineLayout(m_Device, m_DDGIPipelineLayout, nullptr);
  vkDestroyPipeline(m_Device, m_DDGIPipeline, nullptr);

  vmaDestroyBuffer(allocator, m_RaymarchMetadataBuffer,
                   m_RaymarchMetadataBufferAllocation);

  vmaDestroyBuffer(allocator, m_ShadingMetadataBuffer,
                   m_ShadingMetadataBufferAllocation);

  vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
}

VkShaderModule Renderer::CreateShaderModule(const std::string &filename) {

  /**
   * ate: Start reading at the end of the file
   * binary: Read the file as binary file (avoid text transformations)
   */
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open())
    throw std::runtime_error("Failed to open shader file: " + filename);

  /**
   * The advantage of starting to read at the end of the file is that we
   * can use the read position to determine the size of the file and allocate a
   * buffer
   */
  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  /**
   * Seek the beginning of the file and read all bytes
   */
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  VkShaderModuleCreateInfo createInfo{
      /// The type is a shader module create info
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      /// The size of the buffer in bytes
      .codeSize = buffer.size(),
      /// A pointer to the buffer, however, we need to cast it to a
      /// const uint32_t*
      /// And we need to make sure it satisfies the uint32_t alignment
      /// Since we are using an std::vector, the default allocator ensures this.
      .pCode = reinterpret_cast<const uint32_t *>(buffer.data()),
  };

  VkShaderModule shaderModule;

  if (LOG_VK_RESULT(
          vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule)))
    throw std::runtime_error("Failed to create shader module");

  return shaderModule;
}

/// @brief Common

void Renderer::CreateBuffers() {

  // Camera UBO
  {
    VkDeviceSize bufferSize = sizeof(CamearUBO);

    const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

    m_UniformBuffers.resize(framesInFlight);
    m_UniformBuffersMapped.resize(framesInFlight);
    m_UniformBuffersAllocation.resize(framesInFlight);

    for (size_t i = 0; i < framesInFlight; i++) {

      VkBufferCreateInfo bufferInfo = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .size = bufferSize,
          .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      };

      VmaAllocationCreateInfo allocCreateInfo = {
          .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_AUTO,
      };

      VmaAllocationInfo allocInfo;
      vmaCreateBuffer(Akari::Application::GetVmaAllocator(), &bufferInfo,
                      &allocCreateInfo, &m_UniformBuffers[i],
                      &m_UniformBuffersAllocation[i], &allocInfo);

      memset(allocInfo.pMappedData, 0, bufferSize);

      m_UniformBuffersMapped[i] = allocInfo.pMappedData;
    }
  }

  // Raymarch Metadata UBO
  {
    VkDeviceSize bufferSize = sizeof(RaymarchMetadataUBO);

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    vmaCreateBuffer(Akari::Application::GetVmaAllocator(), &bufferInfo,
                    &allocCreateInfo, &m_RaymarchMetadataBuffer,
                    &m_RaymarchMetadataBufferAllocation, nullptr);
  }

  // Shading Metadata UBO
  {
    VkDeviceSize bufferSize = sizeof(ShadingMetadataUBO);

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    vmaCreateBuffer(Akari::Application::GetVmaAllocator(), &bufferInfo,
                    &allocCreateInfo, &m_ShadingMetadataBuffer,
                    &m_ShadingMetadataBufferAllocation, nullptr);
  }
}

void Renderer::CreateDescripterPool() {
  const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

  std::vector<VkDescriptorPoolSize> poolSizes = {
      // DDGI pipline
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, // Metadata
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       5}, // SparseVoxelOctree & m_Material & MaterialLUT & DDGIProbes & Lights
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
       2}, // irradianceAtlas & gaussianDepthAtlas

      // Raymarch pipline
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight}, // Camera
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},              // Metadata
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
       8}, /// albedoTexture & occlusionTexture & directLightTexture &
      /// indirectLightTexture & depthTexture & normalTexture & positionTexture
      /// & materialTexture

      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       2}, // irradianceAtlas & gaussianDepthAtlas
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       5}, /// SparseVoxelOctree & Materials & MaterialLUT & Lights & DDGIProbes

      // Shading pipline
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight}, // Camera
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},              // Metadata
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},               // outputImage
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       8}, /// albedoTexture & occlusionTexture & directLightTexture &
      /// indirectLightTexture & depthTexture & normalTexture & positionTexture
      /// & materialTexture
  };

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = framesInFlight + framesInFlight + 3,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  if (LOG_VK_RESULT(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr,
                                           &m_DescriptorPool)))
    throw std::runtime_error("Failed to create descriptor pool");
}

/// @brief DDGI Pipline

void Renderer::CreateDDGIDescripterSetLayout() {

  std::vector<VkDescriptorSetLayoutBinding> bindings = {

      // Metadata
      VkDescriptorSetLayoutBinding{
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },

      // SparseVoxelOctree
      VkDescriptorSetLayoutBinding{
          .binding = 50,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      // Materials
      VkDescriptorSetLayoutBinding{
          .binding = 51,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      // MaterialLUT
      VkDescriptorSetLayoutBinding{
          .binding = 52,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      // Lights
      VkDescriptorSetLayoutBinding{
          .binding = 54,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },

      // irradianceAtlas
      VkDescriptorSetLayoutBinding{
          .binding = 107,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      // gaussianDepthAtlas
      VkDescriptorSetLayoutBinding{
          .binding = 108,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  if (LOG_VK_RESULT(vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr,
                                                &m_DDGIDescriptorSetLayout)))
    throw std::runtime_error("Failed to create DDGI descriptor set layout");
}

void Renderer::CreateDDGIDescriptorSets() {

  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = m_DescriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &m_DDGIDescriptorSetLayout,
  };

  if (LOG_VK_RESULT(
          vkAllocateDescriptorSets(m_Device, &allocInfo, &m_DDGIDescriptorSet)))
    throw std::runtime_error("Failed to allocate DDGI descriptor sets");

  // --- Uniforms ---
  // Metadata
  VkDescriptorBufferInfo metadataUBOInfo{
      .buffer = m_RaymarchMetadataBuffer,
      .offset = 0,
      .range = sizeof(RaymarchMetadataUBO),
  };

  // --- Storage Buffers ---
  // SparseVoxelOctree
  VkDescriptorBufferInfo svoInfo{
      .buffer = m_SVOBuffer.GetBuffer(),
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };
  // Materials
  VkDescriptorBufferInfo materialInfo{
      .buffer = m_MaterialBuffer.GetBuffer(),
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };
  // MaterialLUT
  VkDescriptorBufferInfo materialLUTInfo{
      .buffer = m_MaterialLUTBuffer.GetBuffer(),
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };
  // Lights
  VkDescriptorBufferInfo lightsInfo{
      .buffer = m_LightBuffer.GetBuffer(),
      .offset = 0,
      .range = VK_WHOLE_SIZE,
  };

  // --- Storage Images ---
  // irradianceAtlas
  VkDescriptorImageInfo ddgiIrradianceAtlasInfo{
      .sampler = VK_NULL_HANDLE,
      .imageView = m_DDGIIrradianceAtlas->m_ImageView,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };
  // gaussianDepthAtlas
  VkDescriptorImageInfo ddgiGaussianDepthInfo{
      .sampler = VK_NULL_HANDLE,
      .imageView = m_DDGIGaussianDepthAtlas->m_ImageView,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  std::vector<VkWriteDescriptorSet> writes{
      // Metadata
      VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m_DDGIDescriptorSet,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &metadataUBOInfo,
      },

      // SparseVoxelOctree
      VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m_DDGIDescriptorSet,
          .dstBinding = 50,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = &svoInfo,
      },
      // Materials
      VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m_DDGIDescriptorSet,
          .dstBinding = 51,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = &materialInfo,
      },
      // MaterialLUT
      VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m_DDGIDescriptorSet,
          .dstBinding = 52,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = &materialLUTInfo,
      },
      // Lights
      VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m_DDGIDescriptorSet,
          .dstBinding = 54,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = &lightsInfo,
      },

      // irradianceAtlas
      VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m_DDGIDescriptorSet,
          .dstBinding = 107,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = &ddgiIrradianceAtlasInfo,
      },
      // gaussianDepthAtlas
      VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m_DDGIDescriptorSet,
          .dstBinding = 108,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = &ddgiGaussianDepthInfo,
      },
  };

  vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);
}

void Renderer::CreateDDGIPipline() {

  const std::string EXE_DIRECTORY = getExecutableDir();

  VkShaderModule computeShaderModule = CreateShaderModule(
      EXE_DIRECTORY + "/../src/Shaders/Raymarch/ddgi.comp.spv");

  VkPipelineShaderStageCreateInfo computeStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = computeShaderModule,
      .pName = "main",
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &m_DDGIDescriptorSetLayout,
  };

  if (LOG_VK_RESULT(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo,
                                           nullptr, &m_DDGIPipelineLayout)))
    throw std::runtime_error("Failed to create DDGI pipeline layout");

  VkComputePipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = computeStage,
      .layout = m_DDGIPipelineLayout,
  };

  if (LOG_VK_RESULT(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1,
                                             &pipelineInfo, nullptr,
                                             &m_DDGIPipeline)))
    throw std::runtime_error("Failed to create the DDGI compute pipeline");

  vkDestroyShaderModule(m_Device, computeShaderModule, nullptr);
}

/// @brief  Raymarch Pipline

void Renderer::CreateRaymarchDescripterSetLayout() {

  std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindings = {
      // Binding 0
      {
          // Metadata
          VkDescriptorSetLayoutBinding{
              .binding = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // albedoTexture
          VkDescriptorSetLayoutBinding{
              .binding = 101,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // occlusionTexture
          VkDescriptorSetLayoutBinding{
              .binding = 102,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // directLightTexture
          VkDescriptorSetLayoutBinding{
              .binding = 103,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // indirectLightTexture
          VkDescriptorSetLayoutBinding{
              .binding = 104,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // depthTexture
          VkDescriptorSetLayoutBinding{
              .binding = 105,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // normalTexture
          VkDescriptorSetLayoutBinding{
              .binding = 106,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // positionTexture
          VkDescriptorSetLayoutBinding{
              .binding = 107,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // materialTexture
          VkDescriptorSetLayoutBinding{
              .binding = 108,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // irradianceAtlas
          VkDescriptorSetLayoutBinding{
              .binding = 157,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // gaussianDepthAtlas
          VkDescriptorSetLayoutBinding{
              .binding = 158,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // SparseVoxelOctree
          VkDescriptorSetLayoutBinding{
              .binding = 50,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // Materials
          VkDescriptorSetLayoutBinding{
              .binding = 51,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // MaterialLUT
          VkDescriptorSetLayoutBinding{
              .binding = 52,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // Lights
          VkDescriptorSetLayoutBinding{
              .binding = 54,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
      // Binding 1
      {
          // Camera
          VkDescriptorSetLayoutBinding{
              .binding = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
  };

  for (size_t i = 0; i < bindings.size(); i++) {
    std::vector<VkDescriptorSetLayoutBinding> &binding = bindings[i];

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(binding.size()),
        .pBindings = binding.data(),
    };

    if (LOG_VK_RESULT(
            vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr,
                                        &m_RaymarchDescriptorSetLayouts[i])))
      throw std::runtime_error(
          "Failed to create raymarch descriptor set layout " +
          std::to_string(i));
  }
}

void Renderer::CreateRaymarchDescriptorSets() {

  // Descriptor Set 0
  {
    m_RaymarchDescriptorSets[0].emplace_back();

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_DescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_RaymarchDescriptorSetLayouts[0],
    };

    if (LOG_VK_RESULT(vkAllocateDescriptorSets(
            m_Device, &allocInfo, &m_RaymarchDescriptorSets[0][0])))
      throw std::runtime_error("Failed to allocate Raymarch descriptor sets 0");

    // --- Uniform Buffers ---
    // Metadata
    VkDescriptorBufferInfo metadataUBOInfo{
        .buffer = m_RaymarchMetadataBuffer,
        .offset = 0,
        .range = sizeof(RaymarchMetadataUBO),
    };

    // --- Storage Images ---
    // albedoTexture
    VkDescriptorImageInfo albedoTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_AlbedoTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    // occlusionTexture
    VkDescriptorImageInfo occlusionTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_OcclusionTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    // directLightTexture
    VkDescriptorImageInfo directLightTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_DirectLightTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    // indirectLightTexture
    VkDescriptorImageInfo indirectLightTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_IndirectLightTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    // depthTexture
    VkDescriptorImageInfo pixelDistanceTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_DepthTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    // normalTexture
    VkDescriptorImageInfo normalTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_NormalTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    // positionTexture
    VkDescriptorImageInfo positionTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_PositionTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    // materialTexture
    VkDescriptorImageInfo materialTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_MaterialTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    // --- Image Samplers ---
    // irradianceAtlas
    VkDescriptorImageInfo irradianceAtlasInfo{
        .sampler = m_DDGIIrradianceAtlas->m_Sampler,
        .imageView = m_DDGIIrradianceAtlas->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // gaussianDepthAtlas
    VkDescriptorImageInfo gaussianDepthAtlasInfo{
        .sampler = m_DDGIGaussianDepthAtlas->m_Sampler,
        .imageView = m_DDGIGaussianDepthAtlas->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    // --- Storage Buffers ---
    // SparseVoxelOctree
    VkDescriptorBufferInfo svoInfo{
        .buffer = m_SVOBuffer.GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    // Materials
    VkDescriptorBufferInfo materialInfo{
        .buffer = m_MaterialBuffer.GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    // MaterialLUT
    VkDescriptorBufferInfo materialLUTInfo{
        .buffer = m_MaterialLUTBuffer.GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    // Lights
    VkDescriptorBufferInfo lightsInfo{
        .buffer = m_LightBuffer.GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    std::vector<VkWriteDescriptorSet> writes{
        // Metadata
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &metadataUBOInfo,
        },

        // albedoTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 101,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &albedoTextureInfo,
        },
        // occlusionTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 102,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &occlusionTextureInfo,
        },
        // directLightTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 103,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &directLightTextureInfo,
        },
        // indirectLightTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 104,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &indirectLightTextureInfo,
        },
        // depthTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 105,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &pixelDistanceTextureInfo,
        },
        // normalTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 106,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &normalTextureInfo,
        },
        // positionTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 107,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &positionTextureInfo,
        },
        // materialTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 108,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &materialTextureInfo,
        },

        // irradianceAtlas
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 157,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &irradianceAtlasInfo,
        },
        // gaussianDepthAtlas
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 158,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &gaussianDepthAtlasInfo,
        },

        // SparseVoxelOctree
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 50,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &svoInfo,
        },
        // Materials
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 51,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &materialInfo,
        },
        // MaterialLUT
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 52,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &materialLUTInfo,
        },
        // Lights
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_RaymarchDescriptorSets[0][0],
            .dstBinding = 54,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &lightsInfo,
        },
    };

    vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }

  // Descriptor Set 1
  {
    const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

    std::vector<VkDescriptorSetLayout> layouts(
        framesInFlight, m_RaymarchDescriptorSetLayouts[1]);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_DescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(framesInFlight),
        .pSetLayouts = layouts.data(),
    };

    m_RaymarchDescriptorSets[1].resize(framesInFlight);

    if (LOG_VK_RESULT(vkAllocateDescriptorSets(
            m_Device, &allocInfo, m_RaymarchDescriptorSets[1].data())))
      throw std::runtime_error("Failed to allocate raymarch descriptor sets 1");

    for (size_t i = 0; i < framesInFlight; i++) {

      // Camera
      VkDescriptorBufferInfo cameraUBOInfo{
          .buffer = m_UniformBuffers[i],
          .offset = 0,
          .range = sizeof(CamearUBO),
      };

      std::vector<VkWriteDescriptorSet> writes{
          // Camera
          VkWriteDescriptorSet{
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet = m_RaymarchDescriptorSets[1][i],
              .dstBinding = 0,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .pBufferInfo = &cameraUBOInfo,
          },
      };

      vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                             writes.data(), 0, nullptr);
    }
  }
}

void Renderer::CreateRaymarchPipline() {

  const std::string EXE_DIRECTORY = getExecutableDir();

  VkShaderModule computeShaderModule = CreateShaderModule(
      EXE_DIRECTORY + "/../src/Shaders/Raymarch/raymarch.comp.spv");

  VkPipelineShaderStageCreateInfo computeStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = computeShaderModule,
      .pName = "main",
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount =
          static_cast<uint32_t>(m_RaymarchDescriptorSetLayouts.size()),
      .pSetLayouts = m_RaymarchDescriptorSetLayouts.data(),
  };

  if (LOG_VK_RESULT(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo,
                                           nullptr, &m_RaymarchPipelineLayout)))
    throw std::runtime_error("Failed to create raymarch pipeline layout");

  VkComputePipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = computeStage,
      .layout = m_RaymarchPipelineLayout,
  };

  if (LOG_VK_RESULT(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1,
                                             &pipelineInfo, nullptr,
                                             &m_RaymarchPipeline)))
    throw std::runtime_error("Failed to create the raymarch compute pipeline");

  vkDestroyShaderModule(m_Device, computeShaderModule, nullptr);
}

/// @brief  Shading Pipline

void Renderer::CreateShadingDescripterSetLayout() {
  std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindings = {
      // Binding 0
      {
          // Metadata
          VkDescriptorSetLayoutBinding{
              .binding = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // outputImage
          VkDescriptorSetLayoutBinding{
              .binding = 100,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // albedoTexture
          VkDescriptorSetLayoutBinding{
              .binding = 150,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // occlusionTexture
          VkDescriptorSetLayoutBinding{
              .binding = 151,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // directLightTexture
          VkDescriptorSetLayoutBinding{
              .binding = 152,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // indirectLightTexture
          VkDescriptorSetLayoutBinding{
              .binding = 153,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // depthTexture
          VkDescriptorSetLayoutBinding{
              .binding = 154,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // normalTexture
          VkDescriptorSetLayoutBinding{
              .binding = 155,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // positionTexture
          VkDescriptorSetLayoutBinding{
              .binding = 158,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // materialTexture
          VkDescriptorSetLayoutBinding{
              .binding = 159,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
      // Binding 1
      {
          // Camera
          VkDescriptorSetLayoutBinding{
              .binding = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
  };

  for (size_t i = 0; i < bindings.size(); i++) {
    std::vector<VkDescriptorSetLayoutBinding> &binding = bindings[i];

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(binding.size()),
        .pBindings = binding.data(),
    };

    if (LOG_VK_RESULT(vkCreateDescriptorSetLayout(
            m_Device, &layoutInfo, nullptr, &m_ShadingDescriptorSetLayouts[i])))
      throw std::runtime_error(
          "Failed to create shading descriptor set layout " +
          std::to_string(i));
  }
}

void Renderer::CreateShadingDescriptorSets() {

  {
    m_ShadingDescriptorSets[0].emplace_back();

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_DescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_ShadingDescriptorSetLayouts[0],
    };

    if (LOG_VK_RESULT(vkAllocateDescriptorSets(m_Device, &allocInfo,
                                               &m_ShadingDescriptorSets[0][0])))
      throw std::runtime_error("Failed to allocate shading descriptor sets");

    // --- Uniform Buffers ---
    // Metadata
    VkDescriptorBufferInfo metadataUBOInfo{
        .buffer = m_ShadingMetadataBuffer,
        .offset = 0,
        .range = sizeof(ShadingMetadataUBO),
    };

    // --- Storage Image ---
    // outputImage
    VkDescriptorImageInfo outImageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_OutputImage->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    // --- Image Sampler ---
    // albedoTexture
    VkDescriptorImageInfo albedoTextureInfo{
        .sampler = m_AlbedoTexture->m_Sampler,
        .imageView = m_AlbedoTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // occlusionTexture
    VkDescriptorImageInfo occlusionTextureInfo{
        .sampler = m_OcclusionTexture->m_Sampler,
        .imageView = m_OcclusionTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // directLightTexture
    VkDescriptorImageInfo directLightTextureInfo{
        .sampler = m_DirectLightTexture->m_Sampler,
        .imageView = m_DirectLightTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // indirectLightTexture
    VkDescriptorImageInfo indirectLightTextureInfo{
        .sampler = m_IndirectLightTexture->m_Sampler,
        .imageView = m_IndirectLightTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // depthTexture
    VkDescriptorImageInfo pixelDistanceTextureInfo{
        .sampler = m_DepthTexture->m_Sampler,
        .imageView = m_DepthTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // normalTexture
    VkDescriptorImageInfo normalTextureInfo{
        .sampler = m_NormalTexture->m_Sampler,
        .imageView = m_NormalTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // positionTexture
    VkDescriptorImageInfo positionTextureInfo{
        .sampler = m_PositionTexture->m_Sampler,
        .imageView = m_PositionTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // materialTexture
    VkDescriptorImageInfo materialTextureInfo{
        .sampler = m_MaterialTexture->m_Sampler,
        .imageView = m_MaterialTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    std::vector<VkWriteDescriptorSet> writes{
        // Metadata
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &metadataUBOInfo,
        },

        // outputImage
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 100,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outImageInfo,
        },

        // albedoTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 150,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &albedoTextureInfo,
        },
        // occlusionTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 151,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &occlusionTextureInfo,
        },
        // directLightTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 152,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &directLightTextureInfo,
        },
        // indirectLightTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 153,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &indirectLightTextureInfo,
        },
        // depthTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 154,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &pixelDistanceTextureInfo,
        },
        // normalTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 155,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &normalTextureInfo,
        },
        // positionTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 158,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &positionTextureInfo,
        },
        // materialTexture
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_ShadingDescriptorSets[0][0],
            .dstBinding = 159,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &materialTextureInfo,
        },
    };

    vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }

  // Descriptor Set 1
  {
    const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

    std::vector<VkDescriptorSetLayout> layouts(
        framesInFlight, m_ShadingDescriptorSetLayouts[1]);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_DescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(framesInFlight),
        .pSetLayouts = layouts.data(),
    };

    m_ShadingDescriptorSets[1].resize(framesInFlight);

    if (LOG_VK_RESULT(vkAllocateDescriptorSets(
            m_Device, &allocInfo, m_ShadingDescriptorSets[1].data())))
      throw std::runtime_error("Failed to allocate shading descriptor sets 1");

    for (size_t i = 0; i < framesInFlight; i++) {

      // Camera
      VkDescriptorBufferInfo cameraUBOInfo{
          .buffer = m_UniformBuffers[i],
          .offset = 0,
          .range = sizeof(CamearUBO),
      };

      std::vector<VkWriteDescriptorSet> writes{
          // Camera
          VkWriteDescriptorSet{
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet = m_ShadingDescriptorSets[1][i],
              .dstBinding = 0,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .pBufferInfo = &cameraUBOInfo,
          },
      };

      vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                             writes.data(), 0, nullptr);
    }
  }
}

void Renderer::CreateShadingPipline() {
  const std::string EXE_DIRECTORY = getExecutableDir();

  VkShaderModule computeShaderModule = CreateShaderModule(
      EXE_DIRECTORY + "/../src/Shaders/Raymarch/shading.comp.spv");

  VkPipelineShaderStageCreateInfo computeStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = computeShaderModule,
      .pName = "main",
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount =
          static_cast<uint32_t>(m_ShadingDescriptorSetLayouts.size()),
      .pSetLayouts = m_ShadingDescriptorSetLayouts.data(),
  };

  if (LOG_VK_RESULT(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo,
                                           nullptr, &m_ShadingPipelineLayout)))
    throw std::runtime_error("Failed to create shading pipeline layout");

  VkComputePipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = computeStage,
      .layout = m_ShadingPipelineLayout,
  };

  if (LOG_VK_RESULT(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1,
                                             &pipelineInfo, nullptr,
                                             &m_ShadingPipeline)))
    throw std::runtime_error("Failed to create the shading compute pipeline");

  vkDestroyShaderModule(m_Device, computeShaderModule, nullptr);
}

/// @brief Action

void Renderer::RecreateDescriptors() {
  vkDeviceWaitIdle(m_Device);
  vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
  CreateDescripterPool();
  CreateDDGIDescriptorSets();
  CreateRaymarchDescriptorSets();
  CreateShadingDescriptorSets();
}

void Renderer::Initialize() {

  m_RaymarchMetadata = {
      .rays = 256,
      .probeSize = 512,
      .probeSpacing = 64,
      .gridResolution = glm::ivec4(8.0f),
      .gridMin = glm::ivec4(-128),
  };

  CreateBuffers();
  CreateDescripterPool();

  CreateDDGIDescripterSetLayout();
  CreateDDGIDescriptorSets();
  CreateDDGIPipline();

  CreateRaymarchDescripterSetLayout();
  CreateRaymarchDescriptorSets();
  CreateRaymarchPipline();

  CreateShadingDescripterSetLayout();
  CreateShadingDescriptorSets();
  CreateShadingPipline();

  m_World->GetPalette().OnFlush([this]() { m_World->GetSVO()->Flush(); });
}

void Renderer::Render() {
  Palette &palette = m_World->GetPalette();
  std::shared_ptr<SparseVoxelOctree> tree = m_World->GetSVO();

  VkCommandBuffer commandBuffer =
      Akari::Application::GetCommandBuffer({.Begin = true});

  bool resized = false;
  std::vector<VkBufferMemoryBarrier2> bufferBarriers = {};

  if (palette.IsDirty()) {
    std::vector<Material> materials = palette.GetMaterials();

    if (m_MaterialBuffer.Upload(commandBuffer, materials))
      resized = true;

    // Build the Material Index lookup table
    {
      uint32_t maxMaterialId = 0;
      for (auto &mat : materials)
        maxMaterialId = mat.Id > maxMaterialId ? mat.Id : maxMaterialId;

      std::vector<uint32_t> materialLUT(maxMaterialId + 1, 0);

      for (size_t i = 0; i < materials.size(); i++)
        materialLUT[materials[i].Id] = i + 1;

      if (m_MaterialLUTBuffer.Upload(commandBuffer, materialLUT))
        resized = true;
    }

    bufferBarriers.emplace_back(m_MaterialBuffer.GetBarrier(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    bufferBarriers.emplace_back(m_MaterialLUTBuffer.GetBarrier(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
  }

  if (tree->IsDirty()) {
    std::vector<FlatVoxel> treeNodes = tree->Flatten();

    std::vector<DenseVoxel> lightNodes = tree->Filter([&palette](Node *node) {
      auto material = palette.GetMaterial(node->Voxel->Material);
      return material && material->Emissive.a > 0.0f;
    });

    if (m_LightBuffer.Upload(commandBuffer, lightNodes))
      resized = true;

    if (m_SVOBuffer.Upload(commandBuffer, treeNodes))
      resized = true;

    {
      const auto &[iaWidth, iaHeight] = SparseVoxelOctree::ComputeDDGIAtlasSize(
          m_RaymarchMetadata.probeSize, 32, m_RaymarchMetadata.iaTilesPerRow,
          m_RaymarchMetadata.iaTilesPerCol);

      m_RaymarchMetadata.iaWidth = iaWidth;
      m_RaymarchMetadata.iaHeight = iaHeight;
      m_DDGIIrradianceAtlas->Resize(iaWidth, iaHeight);

      m_DDGIIrradianceAtlas->m_DescriptorSet =
          (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
              m_DDGIIrradianceAtlas->m_Sampler,
              m_DDGIIrradianceAtlas->m_ImageView,
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

      const auto &[daWidth, daHeight] = SparseVoxelOctree::ComputeDDGIAtlasSize(
          m_RaymarchMetadata.probeSize, 16, m_RaymarchMetadata.daTilesPerRow,
          m_RaymarchMetadata.daTilesPerCol);

      m_RaymarchMetadata.daWidth = daWidth;
      m_RaymarchMetadata.daHeight = daHeight;
      m_DDGIGaussianDepthAtlas->Resize(daWidth, daHeight);

      m_DDGIGaussianDepthAtlas->m_DescriptorSet =
          (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
              m_DDGIGaussianDepthAtlas->m_Sampler,
              m_DDGIGaussianDepthAtlas->m_ImageView,
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    bufferBarriers.emplace_back(m_LightBuffer.GetBarrier(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    bufferBarriers.emplace_back(m_SVOBuffer.GetBarrier(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));

    // Update raymarch metadata buffer
    {
      m_RaymarchMetadata.worldSize = tree->GetSize();

      vmaCopyMemoryToAllocation(
          Akari::Application::GetVmaAllocator(), &m_RaymarchMetadata,
          m_RaymarchMetadataBufferAllocation, 0, sizeof(RaymarchMetadataUBO));
    }
  }

  if (bufferBarriers.size() > 0) {
    VkDependencyInfo depInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount =
            static_cast<uint32_t>(bufferBarriers.size()),
        .pBufferMemoryBarriers = bufferBarriers.data(),
    };

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
  }

  if (resized)
    RecreateDescriptors();

  // Update camera UBO
  {
    CamearUBO data{
        .view = m_Camera->GetViewMatrix(),
        .projection = m_Camera->GetProjectionMatrix(),
        .position = glm::vec4(m_Camera->Position, 0.0f),
        .inverseView = m_Camera->GetInverseViewMatrix(),
        .inverseProjection = m_Camera->GetInverseProjectionMatrix(),
    };

    memcpy(m_UniformBuffersMapped[Akari::Application::GetCurrentFrameIndex()],
           &data, sizeof(CamearUBO));
  }

  m_RaymarchMetadata.frame = Akari::Application::GetFrameCount();

  // Update DDGI UBO
  {
    const uint32_t batchSize = 128;

    m_RaymarchMetadata.worldSize = tree->GetSize();
    uint32_t probes = m_RaymarchMetadata.probeSize;

    uint32_t numBatches =
        static_cast<uint32_t>(std::ceil((probes + batchSize - 1) / batchSize));

    uint32_t batchIndex = m_RaymarchMetadata.frame % numBatches;
    m_RaymarchMetadata.batchStart = batchIndex * batchSize;
    m_RaymarchMetadata.probeBatchSize =
        std::min(batchSize, probes - m_RaymarchMetadata.batchStart);

    vmaCopyMemoryToAllocation(
        Akari::Application::GetVmaAllocator(), &m_RaymarchMetadata,
        m_RaymarchMetadataBufferAllocation, 0, sizeof(RaymarchMetadataUBO));
  }

  // DDGI pipline
  {
    m_DDGIIrradianceAtlas->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                      VK_ACCESS_2_SHADER_WRITE_BIT,
                                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_DDGIGaussianDepthAtlas->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_DDGIPipeline);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_DDGIPipelineLayout, 0, 1, &m_DDGIDescriptorSet, 0,
                            nullptr);

    uint32_t totalThreads =
        m_RaymarchMetadata.probeBatchSize * m_RaymarchMetadata.rays;
    uint32_t localSizeX = 128;
    uint32_t localSizeY = 1;
    uint32_t groupCountX = (totalThreads + localSizeX - 1) / localSizeX;

    vkCmdDispatch(commandBuffer, groupCountX, 1, 1);
  }

  uint32_t groupSizeX = 16;
  uint32_t groupSizeY = 16;

  uint32_t groupCountX =
      (m_OutputImage->GetWidth() + groupSizeX - 1) / groupSizeX;
  uint32_t groupCountY =
      (m_OutputImage->GetHeight() + groupSizeY - 1) / groupSizeY;

  // Raymarch pipline
  {
    m_AlbedoTexture->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                VK_ACCESS_2_SHADER_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_OcclusionTexture->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                   VK_ACCESS_2_SHADER_WRITE_BIT,
                                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_DirectLightTexture->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                     VK_ACCESS_2_SHADER_WRITE_BIT,
                                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_IndirectLightTexture->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                       VK_ACCESS_2_SHADER_WRITE_BIT,
                                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_DDGIIrradianceAtlas->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_DDGIGaussianDepthAtlas->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_DepthTexture->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                               VK_ACCESS_2_SHADER_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_NormalTexture->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                VK_ACCESS_2_SHADER_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_PositionTexture->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_ACCESS_2_SHADER_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_MaterialTexture->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_ACCESS_2_SHADER_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_RaymarchPipeline);

    VkDescriptorSet sets[] = {
        m_RaymarchDescriptorSets[0][0],
        m_RaymarchDescriptorSets[1][Akari::Application::GetCurrentFrameIndex()],
    };

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_RaymarchPipelineLayout, 0, 2, sets, 0, nullptr);

    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Shading pipline
  {
    m_OutputImage->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_2_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_AlbedoTexture->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_OcclusionTexture->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_DirectLightTexture->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_IndirectLightTexture->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_DepthTexture->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_NormalTexture->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_PositionTexture->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_MaterialTexture->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_ShadingPipeline);

    VkDescriptorSet sets[] = {
        m_ShadingDescriptorSets[0][0],
        m_ShadingDescriptorSets[1][Akari::Application::GetCurrentFrameIndex()],
    };

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_ShadingPipelineLayout, 0, 2, sets, 0, nullptr);

    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  m_OutputImage->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  Akari::Application::FlushCommandBuffer(commandBuffer);

  ImGui::Image(m_OutputImage->m_DescriptorSet,
               ImVec2{(float)m_OutputImage->GetWidth(),
                      (float)m_OutputImage->GetHeight()});
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
  if (m_OutputImage->Resize(width, height)) {
    m_OutputImage->m_DescriptorSet =
        (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
            m_OutputImage->m_Sampler, m_OutputImage->m_ImageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_AlbedoTexture->Resize(width, height);
    m_OcclusionTexture->Resize(width, height);
    m_DirectLightTexture->Resize(width, height);
    m_IndirectLightTexture->Resize(width, height);
    m_DepthTexture->Resize(width, height);
    m_NormalTexture->Resize(width, height);
    m_PositionTexture->Resize(width, height);
    m_MaterialTexture->Resize(width, height);

    for (auto &image : m_UIProperties.displayThumbnails) {
      image->m_DescriptorSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
          image->m_Sampler, image->m_ImageView,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    RecreateDescriptors();
  }
}

void Renderer::RenderUI() {
  ImGui::Spacing();
  ImGui::Begin("Textures##Textures");

  VkCommandBuffer commandBuffer =
      Akari::Application::GetCommandBuffer({.Begin = true});

  uint32_t thumbSize = ImGui::GetContentRegionAvail().x;

  // Compute UVs based on zoom & pan

  for (auto &image : m_UIProperties.displayThumbnails) {
    float aspect = (float)image->GetWidth() / (float)image->GetHeight();
    float w = aspect > 1.0f ? thumbSize : thumbSize * aspect;
    float h = aspect > 1.0f ? thumbSize / aspect : thumbSize;

    ImGui::Image(image->m_DescriptorSet, ImVec2(w, h));
  }

  Akari::Application::FlushCommandBuffer(commandBuffer);

  ImGui::End();
  ImGui::Spacing();
}

} // namespace Raymarch
} // namespace Kitagawa