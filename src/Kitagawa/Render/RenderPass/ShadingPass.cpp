#include "ShadingPass.h"

#include "Utility/Utility.h"

namespace Kitagawa {
namespace Render {
namespace RenderPass {

ShadingPass::ShadingPass() : m_Device(Akari::Application::GetDevice()) {}

ShadingPass::~ShadingPass() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();
  vmaDestroyBuffer(allocator, m_MetadataBuffer, m_MetadataAllocation);
}

void ShadingPass::CreateBuffers() {

  // Metadata
  {
    VkDeviceSize bufferSize = sizeof(Metadata);

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
                    &allocCreateInfo, &m_MetadataBuffer, &m_MetadataAllocation,
                    nullptr);
  }
}

void ShadingPass::GetDescriptorPoolSize(
    std::vector<VkDescriptorPoolSize> &pool) {
  const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

  pool.insert(pool.end(), {
                              {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                              {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                              {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
                          });
}

void ShadingPass::CreateDescriptorSetLayout() {
  std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindings = {
      // Binding 0
      {
          // m_MetadataBuffer
          VkDescriptorSetLayoutBinding{
              .binding = Binding::U_METADATA,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // m_DirectLight
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_DIRECT_LIGHT,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Shadow
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_SHADOW,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_AmbientOcclusion
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_AMBIENT_OCCLUSION,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // m_Shading
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_SHADING,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
  };

  m_DescriptorSetLayouts.resize(bindings.size());

  for (size_t i = 0; i < bindings.size(); i++) {
    std::vector<VkDescriptorSetLayoutBinding> &binding = bindings[i];

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(binding.size()),
        .pBindings = binding.data(),
    };

    if (LOG_VK_RESULT(vkCreateDescriptorSetLayout(
            m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayouts[i])))
      throw std::runtime_error("Failed to create descriptor set layout " +
                               std::to_string(i));
  }
}

void ShadingPass::CreateDescriptorSets(VkDescriptorPool descriptorPool) {

  // Set 0
  m_DescriptorSets.emplace_back();

  // Descriptor Set 0
  {
    m_DescriptorSets[0].emplace_back();

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_DescriptorSetLayouts[0],
    };

    if (LOG_VK_RESULT(vkAllocateDescriptorSets(m_Device, &allocInfo,
                                               &m_DescriptorSets[0][0])))
      throw std::runtime_error("Failed to allocate descriptor sets 0");

    // --- Uniform Buffers ---
    // m_MetadataBuffer
    VkDescriptorBufferInfo metadataUBOInfo{
        .buffer = m_MetadataBuffer,
        .offset = 0,
        .range = sizeof(Metadata),
    };

    // --- Sampler Images ---
    // m_DirectLight
    VkDescriptorImageInfo directLightInfo{
        .sampler = m_Init.directLightTexture->m_Sampler,
        .imageView = m_Init.directLightTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // m_Shadow
    VkDescriptorImageInfo shadowInfo{
        .sampler = m_Init.shadowTexture->m_Sampler,
        .imageView = m_Init.shadowTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // m_AmbientOcclusion
    VkDescriptorImageInfo ambientOcclusionInfo{
        .sampler = m_Init.ambientOcclusionTexture->m_Sampler,
        .imageView = m_Init.ambientOcclusionTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    // --- Storage Images ---
    // m_Shading
    VkDescriptorImageInfo shadingInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_Shading->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    std::vector<VkWriteDescriptorSet> writes{
        // m_MetadataBuffer
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::U_METADATA,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &metadataUBOInfo,
        },

        // m_DirectLight
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_DIRECT_LIGHT,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &directLightInfo,
        },
        // m_Shadow
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_SHADOW,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &shadowInfo,
        },
        // m_AmbientOcclusion
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_AMBIENT_OCCLUSION,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &ambientOcclusionInfo,
        },

        // m_Shading
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_SHADING,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &shadingInfo,
        },
    };

    vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }
}

void ShadingPass::CreatePipeline() {

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = static_cast<uint32_t>(m_DescriptorSetLayouts.size()),
      .pSetLayouts = m_DescriptorSetLayouts.data(),
  };

  if (LOG_VK_RESULT(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo,
                                           nullptr, &m_PipelineLayout)))
    throw std::runtime_error("Failed to create pipeline layout");

  VkShaderModule shaderModule = CreateShaderModule(
      getExecutableDir() + "/../src/Shaders/Pipeline/shading.comp.spv");

  VkPipelineShaderStageCreateInfo computeStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = shaderModule,
      .pName = "main",
  };

  VkComputePipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = computeStage,
      .layout = m_PipelineLayout,
  };

  if (LOG_VK_RESULT(vkCreateComputePipelines(
          m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline)))
    throw std::runtime_error("Failed to create the compute pipeline");

  vkDestroyShaderModule(m_Device, shaderModule, nullptr);
}

bool ShadingPass::OnResize(uint32_t width, uint32_t height) {
  return m_Shading->Resize(width, height);
}

void ShadingPass::Render(VkCommandBuffer commandBuffer) {

  Metadata metadata{
      .frame = Akari::Application::GetFrameCount(),
  };

  vmaCopyMemoryToAllocation(Akari::Application::GetVmaAllocator(), &metadata,
                            m_MetadataAllocation, 0, sizeof(Metadata));

  uint32_t groupSizeX = 16;
  uint32_t groupSizeY = 16;

  uint32_t groupCountX = (m_Shading->GetWidth() + groupSizeX - 1) / groupSizeX;
  uint32_t groupCountY = (m_Shading->GetHeight() + groupSizeY - 1) / groupSizeY;

  m_Init.directLightTexture->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_Init.shadowTexture->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_Init.ambientOcclusionTexture->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_Shading->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                        VK_ACCESS_2_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_PipelineLayout, 0, 1, &m_DescriptorSets[0][0], 0,
                          nullptr);

  vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
}

} // namespace RenderPass
} // namespace Render
} // namespace Kitagawa