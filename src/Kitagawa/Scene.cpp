#include "Scene.h"

#include "Application.h"
#include "Binding.h"
#include "Utility/Utility.h"

using namespace Akari::Render;

Scene::Scene() {
  m_DisplayImages = {
      m_Debug,
      m_Normal,
      m_MotionVector,
      m_DirectLight,
  };
}

Scene::~Scene() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();
  VkDevice     device    = Akari::Application::GetDevice();
  vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
  vmaDestroyBuffer(allocator, m_DepthBuffer, m_DepthBufferAllocation);
}

void Scene::Initialize(const InitializeInfo& init) {

  VkResult     result;
  uint32_t     framesInFlight = Akari::Application::GetMaxFramesInFlight();
  VkDevice     device         = Akari::Application::GetDevice();
  VmaAllocator allocator      = Akari::Application::GetVmaAllocator();

  VkDescriptorPoolSize sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
  };

  VkDescriptorPoolCreateInfo poolInfo{
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets       = 2 + (framesInFlight * 3),
      .poolSizeCount = static_cast<uint32_t>(std::size(sizes)),
      .pPoolSizes    = sizes,
  };

  result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool);

  if (result != VK_SUCCESS)
    throw std::runtime_error("Failed to create descriptor pool");

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

    vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &m_DepthBuffer, &m_DepthBufferAllocation, &allocInfo);

    m_DepthBufferPtr = allocInfo.pMappedData;
  }

  m_GBufferPass.CreateRenderPass({
      .attachments = std::vector<RenderPass::AttachmentDescription2>{
          {.format = m_Normal->GetSpecification().Format},
          {.format = m_Material->GetSpecification().Format},
          {.format = m_MotionVector->GetSpecification().Format},
          {.format = m_Debug->GetSpecification().Format},
          {.format = m_Depth->GetSpecification().Format, .depth = true},
      },
  });

  m_GeometryPipeline.CreateDescriptorSetLayout({
      .index          = 0,
      .layoutBindings = {
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::U_CAMERA,
              .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
          },
      },
  });

  m_LightingPipeline.CreateDescriptorSetLayout({
      .index          = 0,
      .layoutBindings = {
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::U_CAMERA,
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
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::T_DEPTH,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Normal
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::T_NORMAL,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_Material
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::T_MATERIAL,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_DirectLight
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::T_DIRECT_LIGHT,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // m_MaterialBuffer
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::S_MATERIALS,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_MaterialLUTBuffer
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::S_MATERIALS_LUT,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_LightBuffer
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::S_LIGHTS,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          // m_SVOBuffer
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::S_SVO,
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
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::T_DIRECT_LIGHT,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },

          // m_Shading
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::T_SHADING,
              .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
          },
      },
  });

  m_DebugPipeline.CreateDescriptorSetLayout({
      .index          = 0,
      .layoutBindings = {
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::U_CAMERA,
              .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT,
          },
      },
  });

  OnResize(init.width, init.height);

  m_GeometryPipeline.CreatePipeline({
      .vertexStride = sizeof(Vertex),
      .attribs      = {
          {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
          {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal)},
          {2, 0, VK_FORMAT_R8_UINT, offsetof(Vertex, Material)},
      },
      .renderPass            = m_GBufferPass.GetRenderPass(),
      .vertexShaderFile      = GetExecutableDir() + "/../src/Shaders/Pipeline2/gBuffer.vert.spv",
      .fragmentShaderFile    = GetExecutableDir() + "/../src/Shaders/Pipeline2/gBuffer.frag.spv",
      .colorBlendAttachments = {
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
          {.blendEnable = VK_FALSE, .colorWriteMask = 0},
      },
      .depthTestEnable  = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
  });

  m_LightingPipeline.CreateComputePipeline({
      .computeShaderFile = GetExecutableDir() + "/../src/Shaders/Pipeline2/lighting.comp.spv",
  });

  m_ShadingPipeline.CreateComputePipeline({
      .computeShaderFile = GetExecutableDir() + "/../src/Shaders/Pipeline2/shading.comp.spv",
  });

  m_DebugPipeline.CreatePipeline({
      .topology     = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
      .vertexStride = sizeof(RayVertex),
      .attribs      = {
          {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(RayVertex, Position)},
          {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(RayVertex, Color)},
      },
      .renderPass            = m_GBufferPass.GetRenderPass(),
      .vertexShaderFile      = GetExecutableDir() + "/../src/Shaders/Pipeline2/debug.vert.spv",
      .fragmentShaderFile    = GetExecutableDir() + "/../src/Shaders/Pipeline2/debug.frag.spv",
      .colorBlendAttachments = {
          {.blendEnable = VK_FALSE, .colorWriteMask = 0},
          {.blendEnable = VK_FALSE, .colorWriteMask = 0},
          {.blendEnable = VK_FALSE, .colorWriteMask = 0},
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
      },
      .depthTestEnable  = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
  });

  m_World->GetPalette().OnFlush([this]() { m_World->GetSVO()->Flush(); });
}

void Scene::Render() {

  uint32_t        framesInFlight = Akari::Application::GetMaxFramesInFlight();
  VkCommandBuffer commandBuffer  = Akari::Application::GetCommandBuffer({.Begin = true});

  glm::vec2 mouse{0};
  glm::vec2 viewport{0};
  GetViewportInfo(mouse.x, mouse.y, viewport.x, viewport.y);

  m_CameraBuffer.Render(m_Camera);

  {
    Palette&                           palette = m_World->GetPalette();
    std::shared_ptr<SparseVoxelOctree> tree    = m_World->GetSVO();

    bool resized = false;

    std::vector<VkBufferMemoryBarrier2> bufferBarriers = {};

    if (palette.IsDirty()) {
      std::vector<Material> materials = palette.GetMaterials();

      if (m_MaterialBuffer.Upload(commandBuffer, materials))
        resized = true;

      // Build the Material Index lookup table
      {
        uint32_t maxMaterialId = 0;
        for (auto& mat : materials)
          maxMaterialId = mat.Id > maxMaterialId ? mat.Id : maxMaterialId;

        std::vector<uint32_t> materialLUT(maxMaterialId + 1, 0);

        for (size_t i = 0; i < materials.size(); i++)
          materialLUT[materials[i].Id] = i + 1;

        if (m_MaterialLUTBuffer.Upload(commandBuffer, materialLUT))
          resized = true;
      }

      bufferBarriers.emplace_back(m_MaterialBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
      bufferBarriers.emplace_back(m_MaterialLUTBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    }

    if (tree->IsDirty()) {
      std::vector<FlatVoxel> treeNodes = tree->Flatten();

      if (m_SVOBuffer.Upload(commandBuffer, treeNodes))
        resized = true;

      std::vector<DenseVoxel> lightNodes = tree->Filter([&palette](Node* node) {
        auto material = palette.GetMaterial(node->Voxel->Material);
        return material && material->Emissive.a > 0.0f;
      });

      if (m_LightBuffer.Upload(commandBuffer, lightNodes))
        resized = true;

      std::vector<Vertex> vertices = tree->GreedyMesh(palette.GetMaterials());

      m_VertexCount       = vertices.size();
      size_t verticesSize = vertices.size() * sizeof(Vertex);
      if (m_VertexBuffer.Upload(commandBuffer, verticesSize, vertices.data()))
        resized = true;

      bufferBarriers.emplace_back(m_SVOBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
      bufferBarriers.emplace_back(m_LightBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
      bufferBarriers.emplace_back(m_VertexBuffer.GetBarrier(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT));
    }

    // Write the depth buffer value at current mouse position to CPU memory
    {
      m_Depth->Transition(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);

      VkBufferImageCopy copyDepth{};
      copyDepth.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      copyDepth.imageSubresource.layerCount = 1;
      copyDepth.imageOffset                 = {(int)mouse.x, (int)mouse.y, 0};
      copyDepth.imageExtent                 = {1, 1, 1};

      vkCmdCopyImageToBuffer(commandBuffer, m_Depth->m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_DepthBuffer, 1, &copyDepth);

      float depth = *reinterpret_cast<float*>(m_DepthBufferPtr);
      float ndcX  = (2.0f * mouse.x) / viewport.x - 1.0f;
      float ndcY  = 1.0f - (2.0f * mouse.y) / viewport.y; // Vulkan Y
      float ndcZ  = depth * 2.0f - 1.0f;

      glm::vec4 clip(ndcX, ndcY, ndcZ, 1.0f);

      glm::vec4 view = m_Camera->GetInverseProjectionMatrix() * clip;
      view /= view.w;

      glm::vec4 world = m_Camera->GetInverseViewMatrix() * view;

      glm::vec3 hitPoint  = glm::vec3(world);
      glm::vec3 rayOrigin = m_Camera->Position;

      std::vector<RayVertex> line = {
          {rayOrigin, {1, 0, 0}},
          {hitPoint, {1, 0, 0}},
      };

      m_DebugVertexBuffer.Upload(commandBuffer, line.size() * sizeof(RayVertex), line.data());
      bufferBarriers.emplace_back(m_DebugVertexBuffer.GetBarrier(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT));
      m_Depth->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
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
      CreateDescriptorSets();
  }

  m_GBufferPass.BeginRenderPass({
      .commandBuffer = commandBuffer,
      .clearColor    = {
          // m_Normal
          {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
          // m_Material
          {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
          // m_MotionVector
          {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
          // m_Debug
          {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
          // m_Depth
          {.depthStencil = {0.0f, 0}},
      },
  });

  //  First pipline
  {
    m_GeometryPipeline.Draw({
        .commandBuffer  = commandBuffer,
        .vertexBuffer   = m_VertexBuffer.GetBuffer(),
        .offsets        = {0},
        .descriptorSets = {
            m_GeometryPipeline.GetDescriptorSet(0, Akari::Application::GetCurrentFrameIndex()),
        },
        .vertexCount = m_VertexCount,
    });

    m_DebugPipeline.Draw({
        .commandBuffer  = commandBuffer,
        .vertexBuffer   = m_DebugVertexBuffer.GetBuffer(),
        .offsets        = {0},
        .descriptorSets = {
            m_DebugPipeline.GetDescriptorSet(0, Akari::Application::GetCurrentFrameIndex()),
        },
        .vertexCount = 6,
    });
  }

  m_Depth->SetCurrentState(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
  m_Normal->SetCurrentState(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
  m_Material->SetCurrentState(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
  m_MotionVector->SetCurrentState(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
  m_Debug->SetCurrentState(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

  vkCmdEndRenderPass(commandBuffer);

  uint32_t groupSizeX = 16;
  uint32_t groupSizeY = 16;

  uint32_t groupCountX =
      (m_OutputImage->GetWidth() + groupSizeX - 1) / groupSizeX;
  uint32_t groupCountY =
      (m_OutputImage->GetHeight() + groupSizeY - 1) / groupSizeY;

  // Lighting compute shader
  {
    m_Depth->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    m_Normal->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    m_Material->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    m_MotionVector->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    m_DirectLight->Transition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    m_LightingPipeline.DispatchCompute({
        .commandBuffer  = commandBuffer,
        .groupCountX    = groupCountX,
        .groupCountY    = groupCountY,
        .descriptorSets = {
            m_LightingPipeline.GetDescriptorSet(0, Akari::Application::GetCurrentFrameIndex()),
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

  m_OutputImage->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
  m_DirectLight->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
  Akari::Application::FlushCommandBuffer(commandBuffer);
  ImGui::Image(m_OutputImage->m_DescriptorSet, ImVec2{(float)m_OutputImage->GetWidth(), (float)m_OutputImage->GetHeight()});
}

void Scene::RenderUI() {
  ImGui::Spacing();
  ImGui::Begin("Textures##Textures");

  VkCommandBuffer commandBuffer =
      Akari::Application::GetCommandBuffer({.Begin = true});

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

  Akari::Application::FlushCommandBuffer(commandBuffer);

  ImGui::End();
  ImGui::Spacing();
}

void Scene::CreateDescriptorSets() {
  uint32_t                                   framesInFlight = Akari::Application::GetMaxFramesInFlight();
  std::vector<Pipeline::DescriptorWriteInfo> cameraWrites(framesInFlight);

  for (size_t i = 0; i < framesInFlight; i++)
    cameraWrites[i] = Pipeline::DescriptorWriteInfo{
        .type    = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .binding = Kitagawa::Binding::U_CAMERA,
        .buffer  = VkDescriptorBufferInfo{
             .buffer = m_CameraBuffer.GetBuffer(i),
             .offset = 0,
             .range  = sizeof(Kitagawa::Render::CameraBuffer::Camera),
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
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = Kitagawa::Binding::T_DIRECT_LIGHT,
                          .image   = VkDescriptorImageInfo{
                                .sampler     = m_DirectLight->m_Sampler,
                                .imageView   = m_DirectLight->m_ImageView,
                                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              },
          },

          // --- Storage Images ---
          // m_OutputImage
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                          .binding = Kitagawa::Binding::T_SHADING,
                          .image   = VkDescriptorImageInfo{
                                .sampler     = VK_NULL_HANDLE,
                                .imageView   = m_OutputImage->m_ImageView,
                                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
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
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = Kitagawa::Binding::T_DEPTH,
                          .image   = VkDescriptorImageInfo{
                                .sampler     = m_Depth->m_Sampler,
                                .imageView   = m_Depth->m_ImageView,
                                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              },
          },
          // m_Normal
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = Kitagawa::Binding::T_NORMAL,
                          .image   = VkDescriptorImageInfo{
                                .sampler     = m_Normal->m_Sampler,
                                .imageView   = m_Normal->m_ImageView,
                                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              },
          },
          // m_Material
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = Kitagawa::Binding::T_MATERIAL,
                          .image   = VkDescriptorImageInfo{
                                .sampler     = m_Material->m_Sampler,
                                .imageView   = m_Material->m_ImageView,
                                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              },
          },

          // --- Storage Images ---
          // m_DirectLight
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                          .binding = Kitagawa::Binding::T_DIRECT_LIGHT,
                          .image   = VkDescriptorImageInfo{
                                .sampler     = VK_NULL_HANDLE,
                                .imageView   = m_DirectLight->m_ImageView,
                                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
              },
          },

          // --- Storage Buffers ---
          // m_MaterialBuffer
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .binding = Kitagawa::Binding::S_MATERIALS,
                          .buffer  = VkDescriptorBufferInfo{
                               .buffer = m_MaterialBuffer.GetBuffer(),
                               .offset = 0,
                               .range  = VK_WHOLE_SIZE,
              },
          },

          // m_MaterialLUTBuffer
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .binding = Kitagawa::Binding::S_MATERIALS_LUT,
                          .buffer  = VkDescriptorBufferInfo{
                               .buffer = m_MaterialLUTBuffer.GetBuffer(),
                               .offset = 0,
                               .range  = VK_WHOLE_SIZE,
              },
          },
          // m_LightBuffer
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .binding = Kitagawa::Binding::S_LIGHTS,
                          .buffer  = VkDescriptorBufferInfo{
                               .buffer = m_LightBuffer.GetBuffer(),
                               .offset = 0,
                               .range  = VK_WHOLE_SIZE,
              },
          },
          // m_SVOBuffer
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                          .binding = Kitagawa::Binding::S_SVO,
                          .buffer  = VkDescriptorBufferInfo{
                               .buffer = m_SVOBuffer.GetBuffer(),
                               .offset = 0,
                               .range  = VK_WHOLE_SIZE,
              },
          },
      },
  });

  m_DebugPipeline.CreateDescriptorSet({
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

  VkDevice device         = Akari::Application::GetDevice();
  uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

  m_Material->Resize(width, height);

  if (m_Debug->Resize(width, height))
    m_Debug->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (m_Normal->Resize(width, height))
    m_Normal->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (m_MotionVector->Resize(width, height))
    m_MotionVector->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (m_DirectLight->Resize(width, height))
    m_DirectLight->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (m_OutputImage->Resize(width, height))
    m_OutputImage->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  VkImageView pAttachments[5] = {
      m_Normal->m_ImageView,
      m_Material->m_ImageView,
      m_MotionVector->m_ImageView,
      m_Debug->m_ImageView,
      m_Depth->m_ImageView,
  };

  m_GBufferPass.CreateFramebuffer({
      .width           = width,
      .height          = height,
      .pAttachments    = pAttachments,
      .attachmentCount = 5,
  });

  CreateDescriptorSets();
}