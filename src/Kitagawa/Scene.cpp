#include "Scene.h"

#include "Application.h"
#include "Binding.h"
#include "Utility/Utility.h"

#include "stb/stb.h"
#include "stb/stb_image.h"

using namespace Akari::Render;

std::vector<OverlayVertex> Scene::MakeVoxelQuad(const glm::vec3& voxelMin, const glm::vec3& hitNormal, const glm::vec3& color, float size) {

  glm::vec3 min = voxelMin;
  glm::vec3 max = voxelMin + glm::vec3(size);

  glm::vec3 v0, v1, v2, v3;

  // Determine face from normal
  if (hitNormal.x > 0.5f) { // +X
    v0 = {max.x, min.y, min.z};
    v1 = {max.x, max.y, min.z};
    v2 = {max.x, max.y, max.z};
    v3 = {max.x, min.y, max.z};
  } else if (hitNormal.x < -0.5f) { // -X
    v0 = {min.x, min.y, max.z};
    v1 = {min.x, max.y, max.z};
    v2 = {min.x, max.y, min.z};
    v3 = {min.x, min.y, min.z};
  } else if (hitNormal.y > 0.5f) { // +Y
    v0 = {min.x, max.y, min.z};
    v1 = {max.x, max.y, min.z};
    v2 = {max.x, max.y, max.z};
    v3 = {min.x, max.y, max.z};
  } else if (hitNormal.y < -0.5f) { // -Y
    v0 = {min.x, min.y, max.z};
    v1 = {max.x, min.y, max.z};
    v2 = {max.x, min.y, min.z};
    v3 = {min.x, min.y, min.z};
  } else if (hitNormal.z > 0.5f) { // +Z
    v0 = {min.x, min.y, max.z};
    v1 = {max.x, min.y, max.z};
    v2 = {max.x, max.y, max.z};
    v3 = {min.x, max.y, max.z};
  } else { // -Z
    v0 = {min.x, max.y, min.z};
    v1 = {max.x, max.y, min.z};
    v2 = {max.x, min.y, min.z};
    v3 = {min.x, min.y, min.z};
  }

  return {{{v0, color},
           {v1, color},
           {v2, color},

           {v0, color},
           {v2, color},
           {v3, color}}};
}

Scene::Scene() {
  m_DisplayImages = {
      m_Normal,
      m_MotionVector,
      m_DirectLight,
  };
}

Scene::~Scene() {
  VmaAllocator allocator = Akari::Application::GetVmaAllocator();
  VkDevice     device    = Akari::Application::GetDevice();
  vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
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
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
  };

  VkDescriptorPoolCreateInfo poolInfo{
      .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets       = 3 + (framesInFlight * 4),
      .poolSizeCount = static_cast<uint32_t>(std::size(sizes)),
      .pPoolSizes    = sizes,
  };

  result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool);

  if (result != VK_SUCCESS)
    throw std::runtime_error("Failed to create descriptor pool");

  m_GBufferPass.CreateRenderPass({
      .attachments = std::vector<RenderPass::AttachmentDescription2>{
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
          // m_Skybox
          VkDescriptorSetLayoutBinding{
              .binding         = Kitagawa::Binding::T_SKYBOX,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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

  m_OverlayPass.CreateRenderPass({
      .attachments = std::vector<RenderPass::AttachmentDescription2>{
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
      .polygonMode  = VK_POLYGON_MODE_FILL,
      .vertexStride = sizeof(Vertex),
      .attribs      = {
          {0, 0, VK_FORMAT_R8_UINT, offsetof(Vertex, Id)},
          {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
          {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal)},
      },
      .renderPass            = m_GBufferPass.GetRenderPass(),
      .vertexShaderFile      = GetExecutableDir() + "/../src/Shaders/Pipeline/gBuffer.vert.spv",
      .fragmentShaderFile    = GetExecutableDir() + "/../src/Shaders/Pipeline/gBuffer.frag.spv",
      .colorBlendAttachments = {
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
          {.blendEnable = VK_FALSE, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},
      },
      .depthTestEnable  = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
  });

  m_LightingPipeline.CreateComputePipeline({
      .computeShaderFile = GetExecutableDir() + "/../src/Shaders/Pipeline/lighting.comp.spv",
  });

  m_ShadingPipeline.CreateComputePipeline({
      .computeShaderFile = GetExecutableDir() + "/../src/Shaders/Pipeline/shading.comp.spv",
  });

  m_OverlayPipeline.CreatePipeline({
      .vertexStride = sizeof(OverlayVertex),
      .attribs      = {
          {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(OverlayVertex, Position)},
          {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(OverlayVertex, Color)},
      },
      .renderPass            = m_OverlayPass.GetRenderPass(),
      .vertexShaderFile      = GetExecutableDir() + "/../src/Shaders/Pipeline/overlay.vert.spv",
      .fragmentShaderFile    = GetExecutableDir() + "/../src/Shaders/Pipeline/overlay.frag.spv",
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
    stbi_uc* bottom = stbi_load((GetExecutableDir() + "/../src/Assets/Skybox/bottom.jpg").c_str(), &width, &height, &channels, STBI_rgb);
    stbi_uc* others = stbi_load((GetExecutableDir() + "/../src/Assets/Skybox/left-right-front-back.jpg").c_str(), &width, &height, &channels, STBI_rgb);
    stbi_uc* top    = stbi_load((GetExecutableDir() + "/../src/Assets/Skybox/top.jpg").c_str(), &width, &height, &channels, STBI_rgb);

    m_Skybox->SetData(others, 0, 0);
    m_Skybox->SetData(others, 0, 1);
    m_Skybox->SetData(top, 0, 2);
    m_Skybox->SetData(bottom, 0, 3);
    m_Skybox->SetData(others, 0, 4);
    m_Skybox->SetData(others, 0, 5);

    VkCommandBuffer commandBuffer = Akari::Application::GetCommandBuffer({.Begin = true});
    m_Skybox->CopyToImage(commandBuffer);
    Akari::Application::FlushCommandBuffer(commandBuffer);

    stbi_image_free(bottom);
    stbi_image_free(others);
    stbi_image_free(top);
  }

  m_World->GetPalette()
      .OnFlush([this]() { m_World->GetSVO()->Flush(); });
}

void Scene::Render() {

  uint32_t        framesInFlight = Akari::Application::GetMaxFramesInFlight();
  VkCommandBuffer commandBuffer  = Akari::Application::GetCommandBuffer({.Begin = true});

  glm::vec2 mouse{0};
  glm::vec2 viewport{0};
  GetViewportInfo(mouse.x, mouse.y, viewport.x, viewport.y);

  m_CameraBuffer.Render(m_Camera);

  SparseOctree<Voxel>* tree = m_World->GetSVO();

  {
    Palette& palette = m_World->GetPalette();

    bool resized = false;

    std::vector<VkBufferMemoryBarrier2> bufferBarriers = {};

    // Build the Material Index lookup table & Material buffer
    if (palette.IsDirty()) {
      std::vector<Material> materials = palette.GetMaterials();

      if (m_MaterialBuffer.Upload(commandBuffer, materials))
        resized = true;

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

    // Build the SVO & light nodes
    if (tree->IsDirty()) {
      auto filterLights = [&palette](SparseOctree<Voxel>::Node* node) {
        auto material = palette.GetMaterial(node->Data->Id);
        return material && material->Emissive.a > 0.0f;
      };

      if (m_LightBuffer.Upload(commandBuffer, tree->Filter(filterLights)))
        resized = true;

      std::vector<SparseOctree<Voxel>::FlatNode> flattened;
      tree->Flatten(flattened);
      if (m_SVOBuffer.Upload(commandBuffer, flattened))
        resized = true;

      std::vector<Vertex> vertices = tree->GreedyMesh(palette.GetMaterials());

      m_VertexCount = vertices.size();
      if (m_VertexBuffer.Upload(commandBuffer, vertices.size() * sizeof(Vertex), vertices.data()))
        resized = true;

      bufferBarriers.emplace_back(m_SVOBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
      bufferBarriers.emplace_back(m_LightBuffer.GetBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
      bufferBarriers.emplace_back(m_VertexBuffer.GetBarrier(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT));
    }

    {
      glm::vec3 rayOrigin = m_Camera->Position;

      float ndcX = (2.0f * mouse.x) / viewport.x - 1.0f;
      float ndcY = (2.0f * mouse.y) / viewport.y - 1.0f;

      glm::vec3 rayDirectionView = glm::normalize(glm::inverse(m_Camera->GetProjectionMatrix()) * glm::vec4(ndcX, ndcY, -1.0f, 1.0f));

      glm::vec3 rayDirection = glm::normalize(glm::mat3(m_Camera->GetInverseViewMatrix()) * rayDirectionView);

      using clock = std::chrono::steady_clock;

      SparseOctree<Voxel>::Hit hit = tree->DeepRaymarch(rayOrigin, rayDirection);

      if (hit.Data) {
        // TMP - SUCCESS!
        bool isCtrlPressed = ImGui::IsKeyPressed(ImGuiKey_LeftCtrl);

        if (isCtrlPressed && ImGui::IsMouseDown(ImGuiMouseButton_Right))
          tree->Clear(hit.Position, hit.Size);

        if (isCtrlPressed && ImGui::IsMouseDown(ImGuiMouseButton_Left))
          tree->Set(hit.Position + hit.Normal, hit.Data, hit.Size);

        auto vertices = MakeVoxelQuad(hit.Position, hit.Normal, glm::vec3(1.0f, 0.0f, 0.0f), hit.Size);

        m_OverlayVertexCount = vertices.size();
        if (m_OverlayVertexBuffer.Upload(commandBuffer, vertices.size() * sizeof(OverlayVertex), vertices.data()))
          resized = true;

        bufferBarriers.emplace_back(m_OverlayVertexBuffer.GetBarrier(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT));
      }
    }

    // Wait for barriers
    if (bufferBarriers.size() > 0) {
      VkDependencyInfo depInfo{
          .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size()),
          .pBufferMemoryBarriers    = bufferBarriers.data(),
      };

      vkCmdPipelineBarrier2(commandBuffer, &depInfo);
    }

    if (resized)
      CreateDescriptorSets();
  }

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

    m_GeometryPipeline.Draw({
        .commandBuffer  = commandBuffer,
        .vertexBuffer   = m_VertexBuffer.GetBuffer(),
        .offsets        = {0},
        .descriptorSets = {
            m_GeometryPipeline.GetDescriptorSet(0, Akari::Application::GetCurrentFrameIndex()),
        },
        .vertexCount = m_VertexCount,
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
    m_Skybox->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

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
            m_OverlayPipeline.GetDescriptorSet(0, Akari::Application::GetCurrentFrameIndex()),
        },
        .vertexCount = m_OverlayVertexCount,
    });

    vkCmdEndRenderPass(commandBuffer);
  }

  m_OutputImage->Transition(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
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
             .range  = sizeof(Kitagawa::CameraBuffer::Camera),
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
          // m_Skybox
          Pipeline::DescriptorWriteInfo{
                          .type    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          .binding = Kitagawa::Binding::T_SKYBOX,
                          .image   = VkDescriptorImageInfo{
                                .sampler     = m_Skybox->m_Sampler,
                                .imageView   = m_Skybox->m_ImageView,
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

  VkDevice device         = Akari::Application::GetDevice();
  uint32_t framesInFlight = Akari::Application::GetMaxFramesInFlight();

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