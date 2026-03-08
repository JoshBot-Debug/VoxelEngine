#include "Scene.h"

#include "Binding.h"
#include "Utility.h"
#include "window/Application.h"

#include "stb/stb.h"
#include "stb/stb_image.h"

using namespace akari::render;
using namespace vxen;

Scene::Scene() {
  m_DisplayImages = {
      m_Normal,
      m_MotionVector,
      m_DirectLight,
  };

  m_SVOBuffer.CreateBuffer();
  m_LightBuffer.CreateBuffer();
  m_MaterialBuffer.CreateBuffer();
  m_MaterialLUTBuffer.CreateBuffer();
  m_VertexBuffer.CreateBuffer();
  m_OverlayVertexBuffer.CreateBuffer();
}

Scene::~Scene() {
  VmaAllocator allocator = akari::window::Application::GetVmaAllocator();
  VkDevice     device    = akari::window::Application::GetDevice();
  vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
}

void Scene::Initialize(const InitializeInfo& init) {

  VkResult     result;
  uint32_t     framesInFlight = akari::window::Application::GetMaxFramesInFlight();
  VkDevice     device         = akari::window::Application::GetDevice();
  VmaAllocator allocator      = akari::window::Application::GetVmaAllocator();

  VkDescriptorPoolSize sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
  };

  VkDescriptorPoolCreateInfo poolInfo {
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets       = 3 + (framesInFlight * 4),
      .poolSizeCount = static_cast<uint32_t>(std::size(sizes)),
      .pPoolSizes    = sizes,
  };

  result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool);

  if (result != VK_SUCCESS)
    throw std::runtime_error("Failed to create descriptor pool");

  m_GBufferPass.CreateRenderPass({
      .attachments = std::vector<RenderPass::AttachmentDescription2> {
          {
              .format      = m_Normal->GetSpecification().Format,
              .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          },
          {
              .format      = m_Material->GetSpecification().Format,
              .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          },
          {
              .format      = m_MotionVector->GetSpecification().Format,
              .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          },
          {
              .format      = m_Depth->GetSpecification().Format,
              .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
              .depth       = true,
          },
      },
  });

  m_GeometryPipeline.CreateDescriptorSetLayout({
      .index          = 0,
      .layoutBindings = {
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::U_CAMERA,
              .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
          },
      },
  });

  m_LightingPipeline.CreateDescriptorSetLayout({
      .index          = 0,
      .layoutBindings = {
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::U_CAMERA,
              .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
  });

  m_LightingPipeline.CreateDescriptorSetLayout({
      .index          = 1,
      .layoutBindings = {
          // m_Depth
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::T_DEPTH,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Normal
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::T_NORMAL,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Material
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::T_MATERIAL,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_DirectLight
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::T_DIRECT_LIGHT,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Skybox
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::T_SKYBOX,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // m_MaterialBuffer
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::S_MATERIALS,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_MaterialLUTBuffer
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::S_MATERIALS_LUT,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_LightBuffer
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::S_LIGHTS,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_SVOBuffer
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::S_SVO,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
  });

  m_ShadingPipeline.CreateDescriptorSetLayout({
      .index          = 0,
      .layoutBindings = {
          // m_DirectLight
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::T_DIRECT_LIGHT,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // m_Shading
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::T_SHADING,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
  });

  m_OverlayPass.CreateRenderPass({
      .attachments = std::vector<RenderPass::AttachmentDescription2> {
          {
              .format        = m_OutputImage->GetSpecification().Format,
              .loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD,
              .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          },
          {
              .format        = m_Depth->GetSpecification().Format,
              .loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD,
              .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
              .finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
              .depth         = true,
          },
      },
  });

  m_OverlayPipeline.CreateDescriptorSetLayout({
      .index          = 0,
      .layoutBindings = {
          // m_Camera
          VkDescriptorSetLayoutBinding {
              .binding         = vxen::Binding::U_CAMERA,
              .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
          },
      },
  });

  OnResize(init.Width, init.Height);

  m_GeometryPipeline.CreatePipeline({
      .polygonMode  = init.PolygonMode,
      .vertexStride = sizeof(Vertex),
      .attribs      = {
          {0, 0, VK_FORMAT_R8_UINT, offsetof(Vertex, Id)},
          {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
          {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal)},
      },
      .renderPass            = m_GBufferPass.GetRenderPass(),
      .vertexShaderFile      = GetExecutableDir() + "/../engine/shaders/pipeline/gBuffer.vert.spv",
      .fragmentShaderFile    = GetExecutableDir() + "/../engine/shaders/pipeline/gBuffer.frag.spv",
      .colorBlendAttachments = {
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
      },
      .depthTestEnable  = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
  });

  m_LightingPipeline.CreateComputePipeline({
      .computeShaderFile = GetExecutableDir() + "/../engine/shaders/pipeline/lighting.comp.spv",
  });

  m_ShadingPipeline.CreateComputePipeline({
      .computeShaderFile = GetExecutableDir() + "/../engine/shaders/pipeline/shading.comp.spv",
  });

  m_OverlayPipeline.CreatePipeline({
      .vertexStride = sizeof(vxen::HighlightVertex),
      .attribs      = {
          {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vxen::HighlightVertex, Position)},
          {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vxen::HighlightVertex, Color)},
      },
      .renderPass            = m_OverlayPass.GetRenderPass(),
      .vertexShaderFile      = GetExecutableDir() + "/../engine/shaders/pipeline/overlay.vert.spv",
      .fragmentShaderFile    = GetExecutableDir() + "/../engine/shaders/pipeline/overlay.frag.spv",
      .colorBlendAttachments = {
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
      },
      .depthTestEnable      = VK_TRUE,
      .depthWriteEnable     = VK_FALSE,
      .depthCompareOp       = VK_COMPARE_OP_GREATER_OR_EQUAL,
      .depthBias            = VK_TRUE,
      .depthBiasSlopeFactor = 2.0f,
      .cullMode             = VK_CULL_MODE_NONE,
  });

  {
    int      width, height, channels;
    stbi_uc* bottom = stbi_load((GetExecutableDir() + "/../engine/assets/skybox/bottom.jpg").c_str(), &width, &height, &channels, STBI_rgb_alpha);
    stbi_uc* others = stbi_load((GetExecutableDir() + "/../engine/assets/skybox/left-right-front-back.jpg").c_str(), &width, &height, &channels, STBI_rgb_alpha);
    stbi_uc* top    = stbi_load((GetExecutableDir() + "/../engine/assets/skybox/top.jpg").c_str(), &width, &height, &channels, STBI_rgb_alpha);

    m_Skybox->SetData(others, 0, 0);
    m_Skybox->SetData(others, 0, 1);
    m_Skybox->SetData(top, 0, 2);
    m_Skybox->SetData(bottom, 0, 3);
    m_Skybox->SetData(others, 0, 4);
    m_Skybox->SetData(others, 0, 5);

    VkCommandBuffer commandBuffer = akari::window::Application::GetCommandBuffer({.Begin = true});
    m_Skybox->CopyToImage(commandBuffer);
    m_Skybox->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 0, 1, 0, 6);
    akari::window::Application::FlushCommandBuffer(commandBuffer);

    stbi_image_free(bottom);
    stbi_image_free(others);
    stbi_image_free(top);
  }
}

void Scene::Render() {

  uint32_t        framesInFlight = akari::window::Application::GetMaxFramesInFlight();
  VkCommandBuffer commandBuffer  = akari::window::Application::GetCommandBuffer({.Begin = true});

  m_CameraBuffer.Render(m_Camera);

  std::vector<VkBufferMemoryBarrier2> bufferBarriers {};
  bool                                bufferResized {false};

  if (TSignal::Consume(0, CHUNK_MANAGER_FLUSH_RENDER)) {
    m_VertexBuffers.clear();
    m_VertexOffsets.clear();
    m_SVOBuffers.clear();
    m_SVOOffsets.clear();

    auto flushedChunks = m_World->FlushRenderer(commandBuffer);

    std::vector<VkDrawIndirectCommand> indirectCommands {};

    for (auto& o : flushedChunks) {
      indirectCommands.emplace_back(VkDrawIndirectCommand {
          .vertexCount   = o.VertexCount,
          .instanceCount = 1,
          .firstVertex   = static_cast<uint32_t>(o.VertexOffset / sizeof(Vertex)),
          .firstInstance = 0,
      });

      bufferBarriers.push_back(o.VertexBarrier);
      bufferBarriers.push_back(o.SVOBarrier);

      m_VertexBuffers.push_back(o.VertexBuffer);
      m_VertexOffsets.push_back(o.VertexOffset);
      m_SVOBuffers.push_back(o.SVOBuffer);
      m_SVOOffsets.push_back(o.SVOOffset);

      std::cout << o.VertexOffset << " " << o.SVOOffset << std::endl;
      bufferResized |= o.VerticesResized || o.SVOResized;
    }

    m_IndirectBuffer.Upload(commandBuffer, indirectCommands.size() * sizeof(VkDrawIndirectCommand), indirectCommands.data());

    bufferBarriers.emplace_back(m_IndirectBuffer.GetBarrier(VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT));

    const std::vector<Material>&                        materials   = m_World->GetMaterials();
    const std::vector<uint32_t>&                        materialLUT = m_World->GetMaterialsLUT();
    const std::vector<SparseOctree<Voxel>::FlatNode>&   svo         = m_World->GetSVO();
    const std::vector<Vertex>&                          vertices    = m_World->GetVertices();
    const std::vector<SparseOctree<Voxel>::FilterNode>& lights      = m_World->GetLights();

    if (bool resized = m_MaterialBuffer.Upload(commandBuffer, materials).Resized || materials.size()) {
      bufferResized = bufferResized || resized;
      bufferBarriers.emplace_back(m_MaterialBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    }

    if (bool resized = m_MaterialLUTBuffer.Upload(commandBuffer, materialLUT).Resized || materialLUT.size()) {
      bufferResized = bufferResized || resized;
      bufferBarriers.emplace_back(m_MaterialLUTBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    }

    if (bool resized = m_LightBuffer.Upload(commandBuffer, lights).Resized || lights.size()) {
      bufferResized = bufferResized || resized;
      bufferBarriers.emplace_back(m_LightBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    }

    if (bool resized = m_SVOBuffer.Upload(commandBuffer, svo).Resized || svo.size()) {
      bufferResized = bufferResized || resized;
      bufferBarriers.emplace_back(m_SVOBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    }

    if (bool resized = m_VertexBuffer.Upload(commandBuffer, vertices.size() * sizeof(Vertex), vertices.data()).Resized || vertices.size()) {
      bufferResized = bufferResized || resized;
      m_VertexCount = vertices.size();
      bufferBarriers.emplace_back(m_VertexBuffer.GetBarrier(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT));
    }

    m_World->Clean();
  }

  if (!m_SVOBuffers.size())
    return;

  {
    auto vertices = m_UI->GetHighlightVertices();

    m_OverlayVertexCount = vertices.size();
    if (bool resized = m_OverlayVertexBuffer.Upload(commandBuffer, vertices.size() * sizeof(vxen::HighlightVertex), vertices.data()).Resized || m_OverlayVertexCount) {
      bufferResized = bufferResized || resized;
      bufferBarriers.emplace_back(m_OverlayVertexBuffer.GetBarrier(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT));
    }
  }

  // Wait for buffer barriers
  if (bufferBarriers.size()) {
    VkDependencyInfo depInfo {
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size()),
        .pBufferMemoryBarriers    = bufferBarriers.data(),
    };

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
  }

  if (bufferResized)
    CreateDescriptorSets();

  // GBuffer pass
  {
    m_GBufferPass.BeginRenderPass({
        .commandBuffer = commandBuffer,
        .clearColor    = {
            // m_Normal
            {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
            // m_Material
            {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
            // m_MotionVector
            {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
            // m_Depth
            {.depthStencil = {0.0f, 0}},
        },
    });

    m_GeometryPipeline.DrawIndirect({
        .commandBuffer  = commandBuffer,
        .vertexBuffers  = m_VertexBuffers,
        .offsets        = std::vector<VkDeviceSize>(m_VertexBuffers.size(), 0),
        .descriptorSets = {
            m_GeometryPipeline.GetDescriptorSet(0, akari::window::Application::GetCurrentFrameIndex()),
        },
        .indirectBuffer = m_IndirectBuffer.GetBuffer(),
        .indirectOffset = 0,
        .drawCount      = static_cast<uint32_t>(m_VertexBuffers.size()),
        .stride         = sizeof(VkDrawIndirectCommand),
    });

    vkCmdEndRenderPass(commandBuffer);
  }

  uint32_t groupSizeX = 16;
  uint32_t groupSizeY = 16;

  uint32_t groupCountX =
      (m_OutputImage->GetWidth() + groupSizeX - 1) / groupSizeX;
  uint32_t groupCountY =
      (m_OutputImage->GetHeight() + groupSizeY - 1) / groupSizeY;

  // Lighting compute shader
  {
    m_DirectLight->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    m_Normal->Transition(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    m_Material->Transition(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    m_MotionVector->Transition(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    m_Depth->Transition(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_LightingPipeline.DispatchCompute({
        .commandBuffer  = commandBuffer,
        .groupCountX    = groupCountX,
        .groupCountY    = groupCountY,
        .descriptorSets = {
            m_LightingPipeline.GetDescriptorSet(0, akari::window::Application::GetCurrentFrameIndex()),
            m_LightingPipeline.GetDescriptorSet(1, 0),
        },
    });
  }

  // Shading/Output image
  {
    m_DirectLight->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    m_OutputImage->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_ShadingPipeline.DispatchCompute({
        .commandBuffer  = commandBuffer,
        .groupCountX    = groupCountX,
        .groupCountY    = groupCountY,
        .descriptorSets = {
            m_ShadingPipeline.GetDescriptorSet(0, 0),
        },
    });
  }

  // Overlay pass
  {
    m_OutputImage->Transition(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    m_Depth->Transition(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

    m_OverlayPass.BeginRenderPass({.commandBuffer = commandBuffer});

    m_OverlayPipeline.Draw({
        .commandBuffer  = commandBuffer,
        .vertexBuffer   = m_OverlayVertexBuffer.GetBuffer(),
        .offsets        = {0},
        .descriptorSets = {
            m_OverlayPipeline.GetDescriptorSet(0, akari::window::Application::GetCurrentFrameIndex()),
        },
        .vertexCount = m_OverlayVertexCount,
    });

    vkCmdEndRenderPass(commandBuffer);
  }

  m_OutputImage->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
  akari::window::Application::FlushCommandBuffer(commandBuffer);
  ImGui::Image(m_OutputImage->m_DescriptorSet, ImVec2 {(float)m_OutputImage->GetWidth(), (float)m_OutputImage->GetHeight()});
}

void Scene::RenderUI() {
  ImGui::Spacing();
  ImGui::Begin("Textures##Textures");

  VkCommandBuffer commandBuffer =
      akari::window::Application::GetCommandBuffer({.Begin = true});

  uint32_t thumbSize = ImGui::GetContentRegionAvail().x;

  for (auto& image : m_DisplayImages) {
    float       aspect = (float)image->GetWidth() / (float)image->GetHeight();
    float       w      = aspect > 1.0f ? thumbSize : thumbSize * aspect;
    float       h      = aspect > 1.0f ? thumbSize / aspect : thumbSize;
    const char* name   = image->GetSpecification().ObjectName;
    if (name)
      ImGui::TextUnformatted(name);

    ImGui::Image(image->m_DescriptorSet, ImVec2(w, h));
  }

  akari::window::Application::FlushCommandBuffer(commandBuffer);

  ImGui::End();
  ImGui::Spacing();
}

void Scene::CreateDescriptorSets() {
  uint32_t                                   framesInFlight = akari::window::Application::GetMaxFramesInFlight();
  std::vector<Pipeline::DescriptorWriteInfo> cameraWrites(framesInFlight);

  std::vector<VkDescriptorBufferInfo> svoBufferInfos {};

  for (size_t i = 0; i < m_SVOBuffers.size(); i++)
  {
    svoBufferInfos.emplace_back(VkDescriptorBufferInfo {
        .buffer = m_SVOBuffers[i],
        .offset = m_SVOOffsets[i],
        .range  = VK_WHOLE_SIZE,
    });
  }

  for (size_t i = 0; i < framesInFlight; i++)
    cameraWrites[i] = Pipeline::DescriptorWriteInfo {
        .type    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .binding = vxen::Binding::U_CAMERA,
        .buffer  = std::vector {
            VkDescriptorBufferInfo {
                 .buffer = m_CameraBuffer.GetBuffer(i),
                 .offset = 0,
                 .range  = sizeof(vxen::CameraBuffer::Camera),
            },
        },
    };

  m_GeometryPipeline.CreateDescriptorSet({
      .id                 = 0,
      .layoutIndex        = 0,
      .descriptorPool     = m_DescriptorPool,
      .descriptorSetCount = framesInFlight,
      .writes             = cameraWrites,
  });

  m_LightingPipeline.CreateDescriptorSet({
      .id                 = 0,
      .layoutIndex        = 0,
      .descriptorPool     = m_DescriptorPool,
      .descriptorSetCount = framesInFlight,
      .writes             = cameraWrites,
  });

  m_ShadingPipeline.CreateDescriptorSet({
      .id                 = 0,
      .layoutIndex        = 0,
      .descriptorPool     = m_DescriptorPool,
      .descriptorSetCount = 1,
      .writes             = {
          // m_DirectLight
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = vxen::Binding::T_DIRECT_LIGHT,
                          .image   = std::vector {
                  VkDescriptorImageInfo {
                                    .sampler     = m_DirectLight->m_Sampler,
                                    .imageView   = m_DirectLight->m_ImageView,
                                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  },
              },
          },

          // --- Storage Images ---
          // m_OutputImage
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                          .binding = vxen::Binding::T_SHADING,
                          .image   = std::vector {
                  VkDescriptorImageInfo {
                                    .sampler     = VK_NULL_HANDLE,
                                    .imageView   = m_OutputImage->m_ImageView,
                                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                  },
              },
          },
      },
  });

  m_LightingPipeline.CreateDescriptorSet({
      .id                 = 1,
      .layoutIndex        = 1,
      .descriptorPool     = m_DescriptorPool,
      .descriptorSetCount = 1,
      .writes             = {
          // --- Sampler Images ---
          // m_Depth
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = vxen::Binding::T_DEPTH,
                          .image   = std::vector {
                  VkDescriptorImageInfo {
                                    .sampler     = m_Depth->m_Sampler,
                                    .imageView   = m_Depth->m_ImageView,
                                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  },
              },
          },
          // m_Normal
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = vxen::Binding::T_NORMAL,
                          .image   = std::vector {
                  VkDescriptorImageInfo {
                                    .sampler     = m_Normal->m_Sampler,
                                    .imageView   = m_Normal->m_ImageView,
                                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  },
              },
          },
          // m_Material
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = vxen::Binding::T_MATERIAL,
                          .image   = std::vector {
                  VkDescriptorImageInfo {
                                    .sampler     = m_Material->m_Sampler,
                                    .imageView   = m_Material->m_ImageView,
                                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  },
              },
          },
          // m_Skybox
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = vxen::Binding::T_SKYBOX,
                          .image   = std::vector {
                  VkDescriptorImageInfo {
                                    .sampler     = m_Skybox->m_Sampler,
                                    .imageView   = m_Skybox->m_ImageView,
                                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  },
              },
          },

          // --- Storage Images ---
          // m_DirectLight
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                          .binding = vxen::Binding::T_DIRECT_LIGHT,
                          .image   = std::vector {
                  VkDescriptorImageInfo {
                                    .sampler     = VK_NULL_HANDLE,
                                    .imageView   = m_DirectLight->m_ImageView,
                                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                  },
              },
          },

          // --- Storage Buffers ---
          // m_MaterialBuffer
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .binding = vxen::Binding::S_MATERIALS,
                          .buffer  = std::vector {
                  VkDescriptorBufferInfo {
                                   .buffer = m_MaterialBuffer.GetBuffer(),
                                   .offset = 0,
                                   .range  = VK_WHOLE_SIZE,
                  },
              },
          },

          // m_MaterialLUTBuffer
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .binding = vxen::Binding::S_MATERIALS_LUT,
                          .buffer  = std::vector {
                  VkDescriptorBufferInfo {
                                   .buffer = m_MaterialLUTBuffer.GetBuffer(),
                                   .offset = 0,
                                   .range  = VK_WHOLE_SIZE,
                  },
              },
          },
          // m_LightBuffer
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .binding = vxen::Binding::S_LIGHTS,
                          .buffer  = std::vector {
                  VkDescriptorBufferInfo {
                                   .buffer = m_LightBuffer.GetBuffer(),
                                   .offset = 0,
                                   .range  = VK_WHOLE_SIZE,
                  },
              },
          },
          // m_SVOBuffer
          Pipeline::DescriptorWriteInfo {
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .binding = vxen::Binding::S_SVO,
                          .buffer  = svoBufferInfos,
          },
      },
  });

  m_OverlayPipeline.CreateDescriptorSet({
      .id                 = 0,
      .layoutIndex        = 0,
      .descriptorPool     = m_DescriptorPool,
      .descriptorSetCount = framesInFlight,
      .writes             = cameraWrites,
  });
}

void Scene::OnResize(uint32_t width, uint32_t height) {

  if (!m_Depth->Resize(width, height))
    return;

  VkDevice device         = akari::window::Application::GetDevice();
  uint32_t framesInFlight = akari::window::Application::GetMaxFramesInFlight();

  m_Material->Resize(width, height);

  if (m_Normal->Resize(width, height))
    m_Normal->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (m_MotionVector->Resize(width, height))
    m_MotionVector->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (m_DirectLight->Resize(width, height))
    m_DirectLight->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (m_OutputImage->Resize(width, height))
    m_OutputImage->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  {
    VkImageView pAttachments[4] = {
        m_Normal->m_ImageView,
        m_Material->m_ImageView,
        m_MotionVector->m_ImageView,
        m_Depth->m_ImageView,
    };

    m_GBufferPass.CreateFramebuffer({
        .width           = width,
        .height          = height,
        .pAttachments    = pAttachments,
        .attachmentCount = 4,
    });
  }

  {
    VkImageView pAttachments[2] = {
        m_OutputImage->m_ImageView,
        m_Depth->m_ImageView,
    };

    m_OverlayPass.CreateFramebuffer({
        .width           = width,
        .height          = height,
        .pAttachments    = pAttachments,
        .attachmentCount = 2,
    });
  }

  CreateDescriptorSets();
}