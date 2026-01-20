#include "ShadowPass.h"

#include "Utility/Utility.h"

namespace Kitagawa {
namespace Render {
namespace RenderPass {

ShadowPass::ShadowPass() : m_Device(Akari::Application::GetDevice()) {}

ShadowPass::~ShadowPass() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();
  vmaDestroyBuffer(allocator, m_MetadataBuffer, m_MetadataAllocation);
}

void ShadowPass::CreateBuffers() {

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

void ShadowPass::GetDescriptorPoolSize(
    std::vector<VkDescriptorPoolSize> &pool) {
  const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

  pool.insert(pool.end(),
              {
                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                  {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                  {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
                  {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
              });
}

void ShadowPass::CreateDescriptorSetLayout() {
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

          // m_Depth
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_DEPTH,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Normal
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_NORMAL,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Material
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_MATERIAL,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Shadow
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_SHADOW,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // m_SVOBuffer
          VkDescriptorSetLayoutBinding{
              .binding = Binding::S_SVO,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_MaterialBuffer
          VkDescriptorSetLayoutBinding{
              .binding = Binding::S_MATERIALS,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_MaterialLUTBuffer
          VkDescriptorSetLayoutBinding{
              .binding = Binding::S_MATERIALS_LUT,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_LightBuffer
          VkDescriptorSetLayoutBinding{
              .binding = Binding::S_LIGHTS,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
      // Binding 1
      {
          // m_Camera
          VkDescriptorSetLayoutBinding{
              .binding = Binding::U_CAMERA,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
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

void ShadowPass::CreateDescriptorSets(VkDescriptorPool descriptorPool) {

  // Set 0
  m_DescriptorSets.emplace_back();
  // Set 1
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
    // m_Depth
    VkDescriptorImageInfo depthInfo{
        .sampler = m_Init.depthTexture->m_Sampler,
        .imageView = m_Init.depthTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // m_Normal
    VkDescriptorImageInfo normalInfo{
        .sampler = m_Init.normalTexture->m_Sampler,
        .imageView = m_Init.normalTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // m_Material
    VkDescriptorImageInfo materialInfo{
        .sampler = m_Init.materialTexture->m_Sampler,
        .imageView = m_Init.materialTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    // --- Storage Images ---
    // m_Shadow
    VkDescriptorImageInfo shadowInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_Shadow->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    // --- Storage Buffers ---
    // m_SVOBuffer
    VkDescriptorBufferInfo svoBufferInfo{
        .buffer = m_Init.svoBuffer->GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    // m_MaterialBuffer
    VkDescriptorBufferInfo materialBufferInfo{
        .buffer = m_Init.materialBuffer->GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    // m_MaterialLUTBuffer
    VkDescriptorBufferInfo materialLUTBufferInfo{
        .buffer = m_Init.materialLUTBuffer->GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    // m_LightBuffer
    VkDescriptorBufferInfo lightBufferInfo{
        .buffer = m_Init.lightBuffer->GetBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
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

        // m_Depth
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_DEPTH,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &depthInfo,
        },
        // m_Normal
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_NORMAL,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &normalInfo,
        },
        // m_Material
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_MATERIAL,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &materialInfo,
        },
        // m_Shadow
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_SHADOW,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &shadowInfo,
        },

        // m_SVOBuffer
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::S_SVO,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &svoBufferInfo,
        },
        // m_MaterialBuffer
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::S_MATERIALS,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &materialBufferInfo,
        },
        // m_MaterialLUTBuffer
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::S_MATERIALS_LUT,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &materialLUTBufferInfo,
        },
        // m_LightBuffer
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::S_LIGHTS,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &lightBufferInfo,
        },
    };

    vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }

  // Descriptor Set 1
  {
    const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight,
                                               m_DescriptorSetLayouts[1]);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(framesInFlight),
        .pSetLayouts = layouts.data(),
    };

    m_DescriptorSets[1].resize(framesInFlight);

    if (LOG_VK_RESULT(vkAllocateDescriptorSets(m_Device, &allocInfo,
                                               m_DescriptorSets[1].data())))
      throw std::runtime_error("Failed to allocate descriptor sets 1");

    for (size_t i = 0; i < framesInFlight; i++) {

      // m_Camera
      VkDescriptorBufferInfo cameraInfo{
          .buffer = m_Init.cameraBuffer->GetBuffer(i),
          .offset = 0,
          .range = sizeof(CameraBuffer::Camera),
      };

      std::vector<VkWriteDescriptorSet> writes{
          // m_Camera
          VkWriteDescriptorSet{
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet = m_DescriptorSets[1][i],
              .dstBinding = Binding::U_CAMERA,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .pBufferInfo = &cameraInfo,
          },
      };

      vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                             writes.data(), 0, nullptr);
    }
  }
}

void ShadowPass::CreatePipeline() {

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = static_cast<uint32_t>(m_DescriptorSetLayouts.size()),
      .pSetLayouts = m_DescriptorSetLayouts.data(),
  };

  if (LOG_VK_RESULT(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo,
                                           nullptr, &m_PipelineLayout)))
    throw std::runtime_error("Failed to create pipeline layout");

  VkShaderModule shaderModule = CreateShaderModule(
      getExecutableDir() + "/../src/Shaders/Pipeline/shadow.comp.spv");

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

bool ShadowPass::OnResize(uint32_t width, uint32_t height)
{
  return m_Shadow->Resize(width, height);
}

void ShadowPass::Render(VkCommandBuffer commandBuffer) {

  Metadata metadata{
      .frame = Akari::Application::GetFrameCount(),
      .worldSize = m_Init.world->GetSVO()->GetSize(),
  };

  vmaCopyMemoryToAllocation(Akari::Application::GetVmaAllocator(), &metadata,
                            m_MetadataAllocation, 0, sizeof(Metadata));

  uint32_t groupSizeX = 16;
  uint32_t groupSizeY = 16;

  uint32_t groupCountX = (m_Shadow->GetWidth() + groupSizeX - 1) / groupSizeX;
  uint32_t groupCountY = (m_Shadow->GetHeight() + groupSizeY - 1) / groupSizeY;

  m_Init.depthTexture->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_Init.normalTexture->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_Init.materialTexture->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_Shadow->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                       VK_ACCESS_2_SHADER_WRITE_BIT,
                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);

  VkDescriptorSet sets[] = {
      m_DescriptorSets[0][0],
      m_DescriptorSets[1][Akari::Application::GetCurrentFrameIndex()],
  };

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_PipelineLayout, 0, 2, sets, 0, nullptr);

  vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
}

} // namespace RenderPass
} // namespace Render
} // namespace Kitagawa