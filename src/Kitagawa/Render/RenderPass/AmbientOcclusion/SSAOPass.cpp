#include "SSAOPass.h"

#include "Utility/Utility.h"

#include <random>

namespace Kitagawa {
namespace Render {
namespace RenderPass {

SSAOPass::SSAOPass() : m_Device(Akari::Application::GetDevice()) {
  VkCommandBuffer commandBuffer =
      Akari::Application::GetCommandBuffer({.Begin = true});

  std::default_random_engine generator;
  std::uniform_real_distribution<float> random(0.0, 1.0);

  // Generate the SSAO kernal
  {
    const uint32_t kernalSize = 64;

    for (uint32_t i = 0; i < kernalSize; ++i) {
      glm::vec4 sample(random(generator) * 2.0f - 1.0f,
                       random(generator) * 2.0f - 1.0f, random(generator),
                       0.0f);
      sample = glm::normalize(sample);
      sample *= random(generator);

      float scale = (float)i / 64.0f;
      scale = lerp(0.1f, 1.0f, scale * scale);
      sample *= scale;

      m_Metadata.kernal[i] = sample;
    }

    m_Metadata.kernelSize = kernalSize;
  }

  // m_Noise
  {
    std::vector<glm::vec4> noise;
    unsigned int pixels = m_Noise->GetWidth() * m_Noise->GetHeight();
    for (unsigned int i = 0; i < pixels; i++)
      noise.push_back(glm::vec4(random(generator) * 2.0 - 1.0,
                                random(generator) * 2.0 - 1.0, 0.0f, 0.0f));

    m_Noise->SetData(noise.data());
    m_Noise->CopyToImage(commandBuffer);
  }

  Akari::Application::FlushCommandBuffer(commandBuffer);
}

SSAOPass::~SSAOPass() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();
  vmaDestroyBuffer(allocator, m_MetadataBuffer, m_MetadataAllocation);
}

void SSAOPass::Initialize(const std::any &info) {
  m_Init = std::any_cast<SSAOPassInit>(info);
  m_BilateralFilterPass.Initialize(BilateralFilterPassInit{
      .inputTexture = m_AmbientOcclusionRaw,
      .depthTexture = m_Init.depthTexture,
      .normalTexture = m_Init.normalTexture,
      .outputSpecification = {
          .Format = VK_FORMAT_R8_UNORM,
          .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
          .ObjectName = "SSAOPass::m_AmbientOcclusionFiltered",
      }});
}

void SSAOPass::CreateBuffers() {
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

  m_BilateralFilterPass.CreateBuffers();
}

void SSAOPass::GetDescriptorPoolSize(std::vector<VkDescriptorPoolSize> &pool) {
  const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

  pool.insert(pool.end(),
              {
                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                  {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
                  {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
                  {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
              });

  m_BilateralFilterPass.GetDescriptorPoolSize(pool);
}

void SSAOPass::CreateDescriptorSetLayout() {
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
          // m_MotionVector
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_MOTION_VECTOR,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_AmbientOcclusionRaw
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_AMBIENT_OCCLUSION,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Noise
          VkDescriptorSetLayoutBinding{
              .binding = Binding::T_SSAO_NOISE,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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

  m_BilateralFilterPass.CreateDescriptorSetLayout();
}

void SSAOPass::CreateDescriptorSets(VkDescriptorPool descriptorPool) {

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
    // m_MotionVector
    VkDescriptorImageInfo motionVectorInfo{
        .sampler = m_Init.motionVectorTexture->m_Sampler,
        .imageView = m_Init.motionVectorTexture->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    // m_Noise
    VkDescriptorImageInfo noiseInfo{
        .sampler = m_Noise->m_Sampler,
        .imageView = m_Noise->m_ImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    // --- Storage Images ---
    // m_AmbientOcclusionRaw
    VkDescriptorImageInfo ambientOcclusionInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_AmbientOcclusionRaw->m_ImageView,
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
        // m_MotionVector
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_MOTION_VECTOR,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &motionVectorInfo,
        },
        // m_AmbientOcclusionRaw
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_AMBIENT_OCCLUSION,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &ambientOcclusionInfo,
        },
        // m_Noise
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_DescriptorSets[0][0],
            .dstBinding = Binding::T_SSAO_NOISE,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &noiseInfo,
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

  m_BilateralFilterPass.CreateDescriptorSets(descriptorPool);
}

void SSAOPass::CreatePipeline() {

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = static_cast<uint32_t>(m_DescriptorSetLayouts.size()),
      .pSetLayouts = m_DescriptorSetLayouts.data(),
  };

  if (LOG_VK_RESULT(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo,
                                           nullptr, &m_PipelineLayout)))
    throw std::runtime_error("Failed to create pipeline layout");

  VkShaderModule shaderModule = CreateShaderModule(
      getExecutableDir() + "/../src/Shaders/Pipeline/ssao.comp.spv");

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

  m_BilateralFilterPass.CreatePipeline();
}

bool SSAOPass::OnResize(uint32_t width, uint32_t height) {
  if (!m_AmbientOcclusionRaw->Resize(width, height))
    return false;
  m_BilateralFilterPass.OnResize(width, height);
  return true;
}

void SSAOPass::Render(VkCommandBuffer commandBuffer) {

  if (m_Init.camera->IsDirty())
    m_Metadata.sampleIndex = 0;

  if (m_Metadata.sampleIndex == 0)
    m_AmbientOcclusionRaw->Clear(commandBuffer, 1.0f, 0.0f, 0.0f, 0.0f);

  m_Metadata.frame = Akari::Application::GetFrameCount(),
  m_Metadata.worldSize = m_Init.world->GetSVO()->GetSize();
  m_Metadata.sampleIndex =
      std::min(m_Metadata.sampleIndex + 1, m_Metadata.kernelSize - 1);

  if (m_Metadata.sampleIndex >= m_Metadata.kernelSize - 1)
    return;

  vmaCopyMemoryToAllocation(Akari::Application::GetVmaAllocator(), &m_Metadata,
                            m_MetadataAllocation, 0, sizeof(Metadata));

  uint32_t groupSizeX = 16;
  uint32_t groupSizeY = 16;

  uint32_t groupCountX =
      (m_AmbientOcclusionRaw->GetWidth() + groupSizeX - 1) / groupSizeX;
  uint32_t groupCountY =
      (m_AmbientOcclusionRaw->GetHeight() + groupSizeY - 1) / groupSizeY;

  m_Init.depthTexture->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_Init.normalTexture->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_Init.motionVectorTexture->Transition(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_AmbientOcclusionRaw->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                    VK_ACCESS_2_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  m_Noise->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_2_SHADER_READ_BIT,
                      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);

  VkDescriptorSet sets[] = {
      m_DescriptorSets[0][0],
      m_DescriptorSets[1][Akari::Application::GetCurrentFrameIndex()],
  };

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_PipelineLayout, 0, 2, sets, 0, nullptr);

  vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

  m_BilateralFilterPass.Render(commandBuffer);
}

} // namespace RenderPass
} // namespace Render
} // namespace Kitagawa