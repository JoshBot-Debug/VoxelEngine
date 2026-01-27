#include "GBufferPass.h"

#include "Utility/Utility.h"

namespace Kitagawa {
namespace Render {
namespace RenderPass {

GBufferPass::GBufferPass() : m_Device(Akari::Application::GetDevice()) {}

GBufferPass::~GBufferPass() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();
  vmaDestroyBuffer(allocator, m_MetadataBuffer, m_MetadataAllocation);
  vmaDestroyBuffer(allocator, m_DepthBuffer, m_DepthBufferAllocation);
}

void GBufferPass::CreateBuffers() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();

  // Metadata
  {
    VkDeviceSize bufferSize = sizeof(Metadata);

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = bufferSize,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &m_MetadataBuffer,
                    &m_MetadataAllocation, nullptr);
  }

  // Depth buffer
  {
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = sizeof(float),
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VmaAllocationCreateInfo allocCreateInfo = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocationInfo allocInfo = {};

    vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &m_DepthBuffer,
                    &m_DepthBufferAllocation, &allocInfo);

    m_DepthBufferPtr = allocInfo.pMappedData;
  }
}

void GBufferPass::GetDescriptorPoolSize(
    std::vector<VkDescriptorPoolSize>& pool) {
  const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

  pool.insert(pool.end(),
              {
                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
              });
}

void GBufferPass::CreateDescriptorSetLayout() {
  std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindings = {
      // Binding 0
      {
          // m_MetadataBuffer
          VkDescriptorSetLayoutBinding{
              .binding         = Binding::U_METADATA,
              .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
          },
      },
      // Binding 1
      {
          // m_Camera
          VkDescriptorSetLayoutBinding{
              .binding         = Binding::U_CAMERA,
              .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
          },
      },
  };

  m_DescriptorSetLayouts.resize(bindings.size());

  for (size_t i = 0; i < bindings.size(); i++) {
    std::vector<VkDescriptorSetLayoutBinding>& binding = bindings[i];

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(binding.size()),
        .pBindings    = binding.data(),
    };

    if (LOG_VK_RESULT(vkCreateDescriptorSetLayout(
            m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayouts[i])))
      throw std::runtime_error("Failed to create descriptor set layout " +
                               std::to_string(i));
  }
}

void GBufferPass::CreateDescriptorSets(VkDescriptorPool descriptorPool) {

  // Set 0
  m_DescriptorSets.emplace_back();
  // Set 1
  m_DescriptorSets.emplace_back();

  // Descriptor Set 0
  {
    m_DescriptorSets[0].emplace_back();

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &m_DescriptorSetLayouts[0],
    };

    if (LOG_VK_RESULT(vkAllocateDescriptorSets(m_Device, &allocInfo,
                                               &m_DescriptorSets[0][0])))
      throw std::runtime_error("Failed to allocate descriptor sets 0");

    // --- Uniform Buffers ---
    // m_MetadataBuffer
    VkDescriptorBufferInfo metadataUBOInfo{
        .buffer = m_MetadataBuffer,
        .offset = 0,
        .range  = sizeof(Metadata),
    };

    std::vector<VkWriteDescriptorSet> writes{
        // m_MetadataBuffer
        VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = m_DescriptorSets[0][0],
            .dstBinding      = Binding::U_METADATA,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo     = &metadataUBOInfo,
        },
    };

    vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
  }

  // Descriptor Set 1
  {
    const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_DescriptorSetLayouts[1]);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(framesInFlight),
        .pSetLayouts        = layouts.data(),
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
          .range  = sizeof(CameraBuffer::Camera),
      };

      std::vector<VkWriteDescriptorSet> writes{
          // m_Camera
          VkWriteDescriptorSet{
              .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet          = m_DescriptorSets[1][i],
              .dstBinding      = Binding::U_CAMERA,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .pBufferInfo     = &cameraInfo,
          },
      };

      vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writes.size()),
                             writes.data(), 0, nullptr);
    }
  }
}

void GBufferPass::CreateRenderPass() {
  // Attachments
  std::vector<VkAttachmentDescription2> attachments{
      // m_Normal
      VkAttachmentDescription2{
          .sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
          .pNext          = nullptr,
          .flags          = 0,
          .format         = m_Normal->GetSpecification().Format,
          .samples        = VK_SAMPLE_COUNT_1_BIT,
          .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      },
      // m_Material
      VkAttachmentDescription2{
          .sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
          .pNext          = nullptr,
          .flags          = 0,
          .format         = m_Material->GetSpecification().Format,
          .samples        = VK_SAMPLE_COUNT_1_BIT,
          .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      },
      // m_MotionVector
      VkAttachmentDescription2{
          .sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
          .pNext          = nullptr,
          .flags          = 0,
          .format         = m_MotionVector->GetSpecification().Format,
          .samples        = VK_SAMPLE_COUNT_1_BIT,
          .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      },
      // m_Depth
      VkAttachmentDescription2{
          .sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
          .pNext          = nullptr,
          .flags          = 0,
          .format         = m_Depth->GetSpecification().Format,
          .samples        = VK_SAMPLE_COUNT_1_BIT,
          .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      },
  };

  // Attachment references
  std::vector<VkAttachmentReference2> colorRefs{
      {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 0,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT},
      {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 1,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT},
      {VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT},
  };

  VkAttachmentReference2 depthRef{
      .sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .pNext      = nullptr,
      .attachment = 3,
      .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
  };

  // Subpass
  VkSubpassDescription2 subpass{
      .sType                   = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
      .pNext                   = nullptr,
      .flags                   = 0,
      .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .viewMask                = 0,
      .inputAttachmentCount    = 0,
      .pInputAttachments       = nullptr,
      .colorAttachmentCount    = static_cast<uint32_t>(colorRefs.size()),
      .pColorAttachments       = colorRefs.data(),
      .pResolveAttachments     = nullptr,
      .pDepthStencilAttachment = &depthRef,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments    = nullptr,
  };

  // Dependencies
  std::vector<VkSubpassDependency2> dependencies{
      VkSubpassDependency2{
          .sType        = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
          .pNext        = nullptr,
          .srcSubpass   = VK_SUBPASS_EXTERNAL,
          .dstSubpass   = 0,
          .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                          VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
          .srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
          .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
          .viewOffset      = 0,
      },
      VkSubpassDependency2{
          .sType        = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
          .pNext        = nullptr,
          .srcSubpass   = 0,
          .dstSubpass   = VK_SUBPASS_EXTERNAL,
          .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
          .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dstAccessMask   = VK_ACCESS_2_SHADER_READ_BIT,
          .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
          .viewOffset      = 0,
      },
  };

  // Render pass info
  VkRenderPassCreateInfo2 rpInfo{
      .sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .pNext                   = nullptr,
      .flags                   = 0,
      .attachmentCount         = static_cast<uint32_t>(attachments.size()),
      .pAttachments            = attachments.data(),
      .subpassCount            = 1,
      .pSubpasses              = &subpass,
      .dependencyCount         = static_cast<uint32_t>(dependencies.size()),
      .pDependencies           = dependencies.data(),
      .correlatedViewMaskCount = 0,
      .pCorrelatedViewMasks    = nullptr,
  };

  if (vkCreateRenderPass2(m_Device, &rpInfo, nullptr, &m_RenderPass) !=
      VK_SUCCESS)
    throw std::runtime_error("Failed to create GBuffer render pass");
}

void GBufferPass::CreateFramebuffer() {
  VkFramebuffer oFramebuffer = m_Framebuffer;

  std::array<VkImageView, 4> attachments = {
      m_Normal->m_ImageView,
      m_Material->m_ImageView,
      m_MotionVector->m_ImageView,
      m_Depth->m_ImageView,
  };

  VkFramebufferCreateInfo framebufferInfo{
      .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass      = m_RenderPass,
      .attachmentCount = static_cast<uint32_t>(attachments.size()),
      .pAttachments    = attachments.data(),
      .width           = m_Depth->GetWidth(),
      .height          = m_Depth->GetHeight(),
      .layers          = 1,
  };

  if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr,
                          &m_Framebuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to create gbuffer framebuffer!");
  }

  vkDestroyFramebuffer(m_Device, oFramebuffer, nullptr);
}

void GBufferPass::CreatePipeline() {

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = static_cast<uint32_t>(m_DescriptorSetLayouts.size()),
      .pSetLayouts    = m_DescriptorSetLayouts.data(),
  };

  if (LOG_VK_RESULT(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo,
                                           nullptr, &m_PipelineLayout)))
    throw std::runtime_error("Failed to create pipeline layout");

  // Create render pass
  CreateRenderPass();

  VkShaderModule vertModule = CreateShaderModule(
      GetExecutableDir() + "/../src/Shaders/Pipeline/gBuffer.vert.spv");
  VkShaderModule fragModule = CreateShaderModule(
      GetExecutableDir() + "/../src/Shaders/Pipeline/gBuffer.frag.spv");

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
      .stride    = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  std::vector<VkVertexInputAttributeDescription> attribs{
      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
      {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal)},
      {2, 0, VK_FORMAT_R8_UINT, offsetof(Vertex, Material)},
  };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount   = 1,
      .pVertexBindingDescriptions      = &bindingDesc,
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribs.size()),
      .pVertexAttributeDescriptions    = attribs.data(),
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
      .renderPass          = m_RenderPass,
      .subpass             = 0,
      .basePipelineHandle  = VK_NULL_HANDLE,
  };

  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &m_Pipeline) != VK_SUCCESS)
    throw std::runtime_error("Failed to create GBuffer graphics pipeline");

  vkDestroyShaderModule(m_Device, fragModule, nullptr);
  vkDestroyShaderModule(m_Device, vertModule, nullptr);
}

bool GBufferPass::OnResize(uint32_t width, uint32_t height) {
  if (!m_Depth->Resize(width, height))
    return false;
  m_Normal->Resize(width, height);
  m_Material->Resize(width, height);
  m_MotionVector->Resize(width, height);
  CreateFramebuffer();
  return true;
}

float* GBufferPass::GetDepth() {
  if (!m_DepthBufferPtr)
    return nullptr;
  return reinterpret_cast<float*>(m_DepthBufferPtr);
}

void GBufferPass::Render(VkCommandBuffer commandBuffer) {

  VmaAllocator allocator = Akari::Application::GetVmaAllocator();

  glm::vec2 mouse{0};
  GetViewportMouse(mouse.x, mouse.y);

  Metadata metadata{
      .frame     = Akari::Application::GetFrameCount(),
      .worldSize = m_Init.world->GetSVO()->GetSize(),
  };

  vmaCopyMemoryToAllocation(allocator, &metadata, m_MetadataAllocation, 0,
                            sizeof(Metadata));

  VkViewport viewport{
      .x        = 0.0f,
      .y        = 0.0f,
      .width    = (float)m_Depth->GetWidth(),
      .height   = (float)m_Depth->GetHeight(),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  m_RenderArea = {
      .offset = {0, 0},
      .extent =
          {
              .width  = m_Depth->GetWidth(),
              .height = m_Depth->GetHeight(),
          },
  };

  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer, 0, 1, &m_RenderArea);

  std::vector<VkClearValue> clearValues = {
      // m_Normal
      {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
      // m_Material
      {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
      // m_MotionVector
      {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
      // m_Depth
      {.depthStencil = {0.0f, 0}},
  };

  VkRenderPassBeginInfo renderPassBeginInfo{
      .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass      = m_RenderPass,
      .framebuffer     = m_Framebuffer,
      .renderArea      = m_RenderArea,
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues    = clearValues.data(),
  };

  vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  //  First pipline
  {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_Pipeline);

    VkDeviceSize offsets[]    = {0};
    VkBuffer     vertexBuffer = m_Init.vertexBuffer->GetBuffer();
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

    VkDescriptorSet sets[] = {
        m_DescriptorSets[0][0],
        m_DescriptorSets[1][Akari::Application::GetCurrentFrameIndex()],
    };

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PipelineLayout, 0, 2, sets, 0, nullptr);

    vkCmdDraw(commandBuffer, m_VertexCount, 1, 0, 0);
  }

  /// TODO: Create another pipline to draw out the rays, etc

  vkCmdEndRenderPass(commandBuffer);

  // Transition the layouts to readonly after all writing is done
  {
    m_Depth->SetCurrentState(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                             VK_ACCESS_2_SHADER_READ_BIT);
    m_Normal->SetCurrentState(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT);
    m_Material->SetCurrentState(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                VK_ACCESS_2_SHADER_READ_BIT);
    m_MotionVector->SetCurrentState(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                    VK_ACCESS_2_SHADER_READ_BIT);

    // Write the depth buffer value at current mouse position to CPU memory
    {
      m_Depth->Transition(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_ACCESS_2_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_2_TRANSFER_BIT);

      VkBufferImageCopy copyDepth{};
      copyDepth.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      copyDepth.imageSubresource.layerCount = 1;
      copyDepth.imageOffset                 = {(int)mouse.x, (int)mouse.y, 0};
      copyDepth.imageExtent                 = {1, 1, 1};

      vkCmdCopyImageToBuffer(commandBuffer, m_Depth->m_Image,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             m_DepthBuffer, 1, &copyDepth);
    }

    m_Depth->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_ACCESS_2_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_Normal->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_Material->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_MotionVector->Transition(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
  }
}

uint32_t GBufferPass::GetMaxSets() {
  const uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();
  // m_Camera(5) + the other set for all other non-per frame buffers
  return framesInFlight + 1;
}

} // namespace RenderPass
} // namespace Render
} // namespace Kitagawa