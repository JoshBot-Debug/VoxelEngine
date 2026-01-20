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
#include <random>

namespace Kitagawa {
namespace Pathtrace {

Renderer::Renderer() {
  m_Device = Akari::Application::GetDevice();

  m_UIProperties.displayThumbnails = {};
}

Renderer::~Renderer() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();

  for (size_t i = 0; i < Akari::Application::GetMaxFramesInFlight(); i++) {
    vmaDestroyBuffer(allocator, m_UniformBuffers[i],
                     m_UniformBuffersAllocation[i]);
  }

  for (const auto &layout : m_PathtraceDescriptorSetLayouts)
    vkDestroyDescriptorSetLayout(m_Device, layout, nullptr);
  vkDestroyPipelineLayout(m_Device, m_PathtracePipelineLayout, nullptr);
  vkDestroyPipeline(m_Device, m_PathtracePipeline, nullptr);

  vmaDestroyBuffer(allocator, m_PathtraceMetadataBuffer,
                   m_PathtraceMetadataBufferAllocation);
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

  // Pathtrace Metadata UBO
  {
    VkDeviceSize bufferSize = sizeof(PathtraceMetadataUBO);

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
                    &allocCreateInfo, &m_PathtraceMetadataBuffer,
                    &m_PathtraceMetadataBufferAllocation, nullptr);
  }
}

void Renderer::CreateDescripterPool() {
  const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

  std::vector<VkDescriptorPoolSize> poolSizes = {
      // Pathtrace pipline
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight}, // Camera
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},              // Metadata
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},               // outputImage
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       3}, // SparseVoxelOctree & Materials & MaterialLUT
  };

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = framesInFlight + 1,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  if (LOG_VK_RESULT(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr,
                                           &m_DescriptorPool)))
    throw std::runtime_error("Failed to create descriptor pool");
}

/// @brief  Pathtrace Pipline

void Renderer::CreatePathtraceDescripterSetLayout() {
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

          //  outputImage
          VkDescriptorSetLayoutBinding{
              .binding = 100,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
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
                                        &m_PathtraceDescriptorSetLayouts[i])))
      throw std::runtime_error(
          "Failed to create Pathtrace descriptor set layout " +
          std::to_string(i));
  }
}

void Renderer::CreatePathtraceDescriptorSets() {

  // Descriptor Set 0
  {
    m_PathtraceDescriptorSets[0].emplace_back();

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_DescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_PathtraceDescriptorSetLayouts[0],
    };

    if (LOG_VK_RESULT(vkAllocateDescriptorSets(
            m_Device, &allocInfo, &m_PathtraceDescriptorSets[0][0])))
      throw std::runtime_error(
          "Failed to allocate pathtrace descriptor sets 0");

    // --- Uniform Buffers ---
    // Metadata
    VkDescriptorBufferInfo metadataUBOInfo{
        .buffer = m_PathtraceMetadataBuffer,
        .offset = 0,
        .range = sizeof(PathtraceMetadataUBO),
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

    // --- Storage Image ---
    // outputImage
    VkDescriptorImageInfo outImageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_OutputImage->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    std::vector<VkWriteDescriptorSet> writes{
        // outputImage
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_PathtraceDescriptorSets[0][0],
            .dstBinding = 100,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outImageInfo,
        },

        // Metadata
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_PathtraceDescriptorSets[0][0],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &metadataUBOInfo,
        },

        // SparseVoxelOctree
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_PathtraceDescriptorSets[0][0],
            .dstBinding = 50,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &svoInfo,
        },
        // Materials
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_PathtraceDescriptorSets[0][0],
            .dstBinding = 51,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &materialInfo,
        },
        // MaterialLUT
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_PathtraceDescriptorSets[0][0],
            .dstBinding = 52,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &materialLUTInfo,
        },
    };

    vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }

  // Descriptor Set 1
  {
    const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

    std::vector<VkDescriptorSetLayout> layouts(
        framesInFlight, m_PathtraceDescriptorSetLayouts[1]);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_DescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(framesInFlight),
        .pSetLayouts = layouts.data(),
    };

    m_PathtraceDescriptorSets[1].resize(framesInFlight);

    if (LOG_VK_RESULT(vkAllocateDescriptorSets(
            m_Device, &allocInfo, m_PathtraceDescriptorSets[1].data())))
      throw std::runtime_error(
          "Failed to allocate pathtrace descriptor sets 1");

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
              .dstSet = m_PathtraceDescriptorSets[1][i],
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

void Renderer::CreatePathtracePipline() {

  const std::string EXE_DIRECTORY = getExecutableDir();

  VkShaderModule computeShaderModule = CreateShaderModule(
      EXE_DIRECTORY + "/../src/Shaders/Pathtrace/pathtrace.comp.spv");

  VkPipelineShaderStageCreateInfo computeStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = computeShaderModule,
      .pName = "main",
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount =
          static_cast<uint32_t>(m_PathtraceDescriptorSetLayouts.size()),
      .pSetLayouts = m_PathtraceDescriptorSetLayouts.data(),
  };

  if (LOG_VK_RESULT(vkCreatePipelineLayout(
          m_Device, &pipelineLayoutInfo, nullptr, &m_PathtracePipelineLayout)))
    throw std::runtime_error("Failed to create pathtrace pipeline layout");

  VkComputePipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = computeStage,
      .layout = m_PathtracePipelineLayout,
  };

  if (LOG_VK_RESULT(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1,
                                             &pipelineInfo, nullptr,
                                             &m_PathtracePipeline)))
    throw std::runtime_error("Failed to create the pathtrace compute pipeline");

  vkDestroyShaderModule(m_Device, computeShaderModule, nullptr);
}

/// @brief Action

void Renderer::RecreateDescriptors() {
  vkDeviceWaitIdle(m_Device);
  vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
  CreateDescripterPool();
  CreatePathtraceDescriptorSets();
}

void Renderer::Initialize() {
  CreateBuffers();
  CreateDescripterPool();

  CreatePathtraceDescripterSetLayout();
  CreatePathtraceDescriptorSets();
  CreatePathtracePipline();

  {
    VkCommandBuffer commandBuffer =
        Akari::Application::GetCommandBuffer({.Begin = true});

    m_World->GetPalette().OnFlush([this]() {
      std::shared_ptr<SparseVoxelOctree> tree = m_World->GetSVO();

      tree->Flush();
      m_MetadataUBO.sampleIndex = 0;

      VkCommandBuffer commandBuffer =
          Akari::Application::GetCommandBuffer({.Begin = true});
      m_OutputImage->Clear(commandBuffer, 0.0f, 0.0f, 0.0f, 0.0f);
      Akari::Application::FlushCommandBuffer(commandBuffer);
    });

    Akari::Application::FlushCommandBuffer(commandBuffer);
  }
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

    m_MetadataUBO.materialSize = static_cast<uint32_t>(materials.size());

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

    m_MetadataUBO.worldSize = static_cast<uint32_t>(tree->GetSize());

    if (m_SVOBuffer.Upload(commandBuffer, treeNodes))
      resized = true;

    bufferBarriers.emplace_back(m_SVOBuffer.GetBarrier(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
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

  if (m_Camera->IsDirty()) {
    m_MetadataUBO.sampleIndex = 0;
    m_OutputImage->Clear(commandBuffer, 0.0f, 0.0f, 0.0f, 0.0f);
  }

  if (resized)
    RecreateDescriptors();

  // Update metadata
  {
    m_MetadataUBO.sampleIndex += 1;
    vmaCopyMemoryToAllocation(
        Akari::Application::GetVmaAllocator(), &m_MetadataUBO,
        m_PathtraceMetadataBufferAllocation, 0, sizeof(PathtraceMetadataUBO));
  }

  // Update camera UBO
  {
    CamearUBO data{
        .inverseView = m_Camera->GetInverseViewMatrix(),
        .inverseProjection = m_Camera->GetInverseProjectionMatrix(),
    };

    memcpy(m_UniformBuffersMapped[Akari::Application::GetCurrentFrameIndex()],
           &data, sizeof(CamearUBO));
  }

  if (m_Camera->IsDirty())
    m_OutputImage->Clear(commandBuffer, 0.0f, 0.0f, 0.0f, 0.0f);

  uint32_t groupSizeX = 16;
  uint32_t groupSizeY = 16;

  uint32_t groupCountX =
      (m_OutputImage->GetWidth() + groupSizeX - 1) / groupSizeX;
  uint32_t groupCountY =
      (m_OutputImage->GetHeight() + groupSizeY - 1) / groupSizeY;

  // Pathtrace pipline
  {
    m_OutputImage->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_2_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_PathtracePipeline);

    VkDescriptorSet sets[] = {
        m_PathtraceDescriptorSets[0][0],
        m_PathtraceDescriptorSets[1]
                                 [Akari::Application::GetCurrentFrameIndex()],
    };

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_PathtracePipelineLayout, 0, 2, sets, 0, nullptr);

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
    m_MetadataUBO.sampleIndex = 0;

    VkCommandBuffer commandBuffer =
        Akari::Application::GetCommandBuffer({.Begin = true});

    m_OutputImage->Clear(commandBuffer, 0.0f, 0.0f, 0.0f, 0.0f);

    Akari::Application::FlushCommandBuffer(commandBuffer);

    m_OutputImage->m_DescriptorSet =
        (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
            m_OutputImage->m_Sampler, m_OutputImage->m_ImageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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

} // namespace Pathtrace
} // namespace Kitagawa