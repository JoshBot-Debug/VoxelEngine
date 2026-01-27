#include "Pipeline.h"

#include <fstream>

#include "Application.h"

namespace Akari {
namespace Render {

VkShaderModule Pipeline::CreateShaderModule(const std::string& filename) {
  VkDevice device = Akari::Application::GetDevice();

  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open())
    throw std::runtime_error("Failed to open shader file: " + filename);

  size_t            fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  VkShaderModuleCreateInfo createInfo{
      .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = buffer.size(),
      .pCode    = reinterpret_cast<const uint32_t*>(buffer.data()),
  };

  VkShaderModule shaderModule;

  VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);

  if (result != VK_SUCCESS)
    throw std::runtime_error("Failed to create shader module");

  return shaderModule;
}

Pipeline::~Pipeline() {
  VkDevice device = Akari::Application::GetDevice();

  vkDestroyPipeline(device, m_Pipeline, nullptr);

  vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);

  for (size_t i = 0; i < m_DescriptorSetLayouts.size(); i++)
    vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayouts[i], nullptr);
}

void Pipeline::CreateDescriptorSetLayout(const DescriptorSetLayoutInfo& info) {
  VkDevice device = Akari::Application::GetDevice();

  if (m_DescriptorSetLayouts.size() <= info.index)
    m_DescriptorSetLayouts.resize(info.index + 1);

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(info.layoutBindings.size()),
      .pBindings    = info.layoutBindings.data(),
  };

  VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayouts[info.index]);

  if (result != VK_SUCCESS)
    throw std::runtime_error("Failed to create descriptor set layout");
}

void Pipeline::CreateDescriptorSet(const DescriptorSetInfo& info) {
  VkDevice device = Akari::Application::GetDevice();

  VkDescriptorSetLayout&        layout         = m_DescriptorSetLayouts[info.layoutIndex];
  std::vector<VkDescriptorSet>& descriptorSets = m_DescriptorSets[info.id];

  // Allocate the descriptor set
  if (descriptorSets.size() == 0) {
    std::vector<VkDescriptorSetLayout> layouts(info.descriptorSetCount, layout);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = info.descriptorPool,
        .descriptorSetCount = info.descriptorSetCount,
        .pSetLayouts        = layouts.data(),
    };

    descriptorSets.resize(info.descriptorSetCount);

    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data());

    if (result != VK_SUCCESS)
      throw std::runtime_error("Failed to allocate descriptor sets");
  }

  // Update descriptor sets
  {
    for (size_t i = 0; i < info.descriptorSetCount; i++) {
      std::vector<VkWriteDescriptorSet> writes;

      for (auto& w : info.writes) {
        VkWriteDescriptorSet write = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = descriptorSets[i],
            .dstBinding      = w.binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = w.type,
        };
        if (w.buffer.has_value())
          write.pBufferInfo = &*w.buffer;
        if (w.image.has_value())
          write.pImageInfo = &*w.image;
        writes.push_back(write);
      }

      vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
  }
}

void Pipeline::CreatePipeline(const PipelineInfo& info) {
  VkDevice device = Akari::Application::GetDevice();

  VkResult result = VK_ERROR_UNKNOWN;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = static_cast<uint32_t>(m_DescriptorSetLayouts.size()),
      .pSetLayouts    = m_DescriptorSetLayouts.data(),
  };

  result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout);

  if (result != VK_SUCCESS)
    throw std::runtime_error("Failed to create pipeline layout");

  VkShaderModule vertModule = CreateShaderModule(info.vertexShaderFile);
  VkShaderModule fragModule = CreateShaderModule(info.fragmentShaderFile);

  VkPipelineShaderStageCreateInfo vertStage{
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertModule,
      .pName  = "main",
  };
  VkPipelineShaderStageCreateInfo fragStage{
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragModule,
      .pName  = "main",
  };
  VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

  VkVertexInputBindingDescription bindingDesc{
      .binding   = 0,
      .stride    = info.vertexStride,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount   = 1,
      .pVertexBindingDescriptions      = &bindingDesc,
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(info.attribs.size()),
      .pVertexAttributeDescriptions    = info.attribs.data(),
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkPipelineViewportStateCreateInfo viewportState{
      .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports    = nullptr,
      .scissorCount  = 1,
      .pScissors     = nullptr,
  };

  std::array<VkDynamicState, 2> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamicStateInfo{
      .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates    = dynamicStates.data(),
  };

  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable        = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode             = VK_POLYGON_MODE_FILL,
      // .polygonMode = VK_POLYGON_MODE_LINE,
      .cullMode        = VK_CULL_MODE_BACK_BIT,
      .frontFace       = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth       = 2.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable  = VK_FALSE,
  };

  VkPipelineDepthStencilStateCreateInfo depthStencil{
      .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable       = VK_TRUE,
      .depthWriteEnable      = VK_TRUE,
      .depthCompareOp        = VK_COMPARE_OP_GREATER,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable     = VK_FALSE,
  };

  std::array<VkPipelineColorBlendAttachmentState, 3> colorBlendAttachments{};
  for (auto& cb : colorBlendAttachments) {
    cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cb.blendEnable = VK_FALSE;
  }

  VkPipelineColorBlendStateCreateInfo colorBlending{
      .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable   = VK_FALSE,
      .attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
      .pAttachments    = colorBlendAttachments.data(),
  };

  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount          = 2,
      .pStages             = shaderStages,
      .pVertexInputState   = &vertexInputInfo,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState      = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState   = &multisampling,
      .pDepthStencilState  = &depthStencil,
      .pColorBlendState    = &colorBlending,
      .pDynamicState       = &dynamicStateInfo,
      .layout              = m_PipelineLayout,
      .renderPass          = info.renderPass,
      .subpass             = 0,
      .basePipelineHandle  = VK_NULL_HANDLE,
  };

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
    throw std::runtime_error("Failed to create GBuffer graphics pipeline");

  vkDestroyShaderModule(device, fragModule, nullptr);
  vkDestroyShaderModule(device, vertModule, nullptr);
}

void Pipeline::CreateComputePipeline(const ComputePipelineInfo& info) {

  VkDevice device = Akari::Application::GetDevice();
  VkResult result = VK_ERROR_UNKNOWN;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = static_cast<uint32_t>(m_DescriptorSetLayouts.size()),
      .pSetLayouts    = m_DescriptorSetLayouts.data(),
  };

  result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout);
  if (result != VK_SUCCESS)
    throw std::runtime_error("Failed to create pipeline layout");

  VkShaderModule shaderModule = CreateShaderModule(info.computeShaderFile);

  VkPipelineShaderStageCreateInfo computeStage{
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = shaderModule,
      .pName  = "main",
  };

  VkComputePipelineCreateInfo pipelineInfo{
      .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage  = computeStage,
      .layout = m_PipelineLayout,
  };

  result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);

  if (result != VK_SUCCESS)
    throw std::runtime_error("Failed to create the compute pipeline");

  vkDestroyShaderModule(device, shaderModule, nullptr);
}

void Pipeline::DispatchCompute(const DispatchComputeInfo& info) {
  vkCmdBindPipeline(info.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
  vkCmdBindDescriptorSets(info.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout, 0, static_cast<uint32_t>(info.descriptorSets.size()), info.descriptorSets.data(), 0, nullptr);
  vkCmdDispatch(info.commandBuffer, info.groupCountX, info.groupCountY, info.groupCountZ);
}

void Pipeline::Draw(const DrawInfo& info) {
  vkCmdBindPipeline(info.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
  vkCmdBindVertexBuffers(info.commandBuffer, 0, 1, &info.vertexBuffer, info.offsets.data());
  vkCmdBindDescriptorSets(info.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, static_cast<uint32_t>(info.descriptorSets.size()), info.descriptorSets.data(), 0, nullptr);
  vkCmdDraw(info.commandBuffer, info.vertexCount, 1, 0, 0);
}

VkDescriptorSet Pipeline::GetDescriptorSet(uint32_t id, uint32_t index) {
  auto it = m_DescriptorSets.find(id);
  if (it == m_DescriptorSets.end() || index >= it->second.size() || it->second[index] == VK_NULL_HANDLE) {
    throw std::runtime_error("Invalid/missing descriptor set " + std::to_string(id) + "[" + std::to_string(index) + "]");
  }
  return it->second[index];
}

} // namespace Render
} // namespace Akari