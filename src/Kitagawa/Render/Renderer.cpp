#include "Renderer.h"

#include "imgui_impl_vulkan.h"

namespace Kitagawa {

void Renderer::CreateDescripterPool(
    const std::vector<VkDescriptorPoolSize> &pool) {

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = m_GBufferPass.GetMaxSets() + m_LightingPass.GetMaxSets() +
                 m_ShadingPass.GetMaxSets(),
      .poolSizeCount = static_cast<uint32_t>(pool.size()),
      .pPoolSizes = pool.data(),
  };

  if (LOG_VK_RESULT(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr,
                                           &m_DescriptorPool)))
    throw std::runtime_error("Failed to create descriptor pool");
}

void Renderer::CreateDescriptorSets() {
  vkDeviceWaitIdle(m_Device);
  vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

  std::vector<VkDescriptorPoolSize> pool = {};
  m_GBufferPass.GetDescriptorPoolSize(pool);
  m_LightingPass.GetDescriptorPoolSize(pool);
  m_ShadingPass.GetDescriptorPoolSize(pool);
  CreateDescripterPool(pool);

  m_GBufferPass.CreateDescriptorSets(m_DescriptorPool);
  m_LightingPass.CreateDescriptorSets(m_DescriptorPool);
  m_ShadingPass.CreateDescriptorSets(m_DescriptorPool);
}

Renderer::Renderer() {
  m_Device = Akari::Application::GetDevice();

  m_DisplayImages = {
      m_GBufferPass.GetTexture(Binding::T_DEPTH),
      m_GBufferPass.GetTexture(Binding::T_NORMAL),
      m_GBufferPass.GetTexture(Binding::T_MOTION_VECTOR),
      m_LightingPass.GetTexture(Binding::T_DIRECT_LIGHT),
  };
}

Renderer::~Renderer() {
  vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
}

void Renderer::Initialize() {

  m_GBufferPass.Initialize(RenderPass::GBufferPassInit{
      .world = m_World,
      .vertexBuffer = &m_VertexBuffer,
      .cameraBuffer = &m_CameraBuffer,
  });

  m_LightingPass.Initialize(RenderPass::LightingPassInit{
      .world = m_World,
      .lightBuffer = &m_LightBuffer,
      .materialBuffer = &m_MaterialBuffer,
      .materialLUTBuffer = &m_MaterialLUTBuffer,
      .cameraBuffer = &m_CameraBuffer,
      .depthTexture = m_GBufferPass.GetTexture(Binding::T_DEPTH),
      .normalTexture = m_GBufferPass.GetTexture(Binding::T_NORMAL),
      .materialTexture = m_GBufferPass.GetTexture(Binding::T_MATERIAL),
  });

  m_ShadingPass.Initialize(RenderPass::ShadingPassInit{
      .world = m_World,
      .directLightTexture = m_LightingPass.GetTexture(Binding::T_DIRECT_LIGHT),
  });

  m_GBufferPass.CreateBuffers();
  m_LightingPass.CreateBuffers();
  m_ShadingPass.CreateBuffers();

  m_GBufferPass.CreateDescriptorSetLayout();
  m_LightingPass.CreateDescriptorSetLayout();
  m_ShadingPass.CreateDescriptorSetLayout();

  CreateDescriptorSets();

  m_GBufferPass.CreatePipeline();
  m_LightingPass.CreatePipeline();
  m_ShadingPass.CreatePipeline();

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

    if (m_SVOBuffer.Upload(commandBuffer, treeNodes))
      resized = true;

    std::vector<DenseVoxel> lightNodes = tree->Filter([&palette](Node *node) {
      auto material = palette.GetMaterial(node->Voxel->Material);
      return material && material->Emissive.a > 0.0f;
    });

    if (m_LightBuffer.Upload(commandBuffer, lightNodes))
      resized = true;

    std::vector<Vertex> vertices = tree->GreedyMesh(palette.GetMaterials());

    // for (size_t i = 0; i < vertices.size(); i++)
    // {
    //   LOG_IVEC3("Position", vertices[i].Position);
    //   // if(vertices[i].Position.x > 64) std::cout << "SUS x " <<
    //   vertices[i].Position.x << std::endl;
    //   // if(vertices[i].Position.y > 64) std::cout << "SUS y " <<
    //   vertices[i].Position.y << std::endl;
    //   // if(vertices[i].Position.z > 64) std::cout << "SUS z " <<
    //   vertices[i].Position.z << std::endl;
    // }

    m_GBufferPass.m_VertexCount = vertices.size();
    size_t verticesSize = vertices.size() * sizeof(Vertex);
    if (m_VertexBuffer.Upload(commandBuffer, verticesSize, vertices.data()))
      resized = true;

    bufferBarriers.emplace_back(m_SVOBuffer.GetBarrier(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    bufferBarriers.emplace_back(m_LightBuffer.GetBarrier(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    bufferBarriers.emplace_back(m_VertexBuffer.GetBarrier(
        VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
        VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT));
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

  m_CameraBuffer.Render(m_Camera);

  m_GBufferPass.Render(commandBuffer);
  m_LightingPass.Render(commandBuffer);
  m_ShadingPass.Render(commandBuffer);

  Akari::Application::FlushCommandBuffer(commandBuffer);

  auto output = m_LightingPass.GetTexture(Binding::T_DIRECT_LIGHT);

  ImGui::Image(output->m_DescriptorSet,
               ImVec2{(float)output->GetWidth(), (float)output->GetHeight()});
}

void Renderer::OnResize(uint32_t width, uint32_t height) {

  if (m_GBufferPass.OnResize(width, height)) {
    m_LightingPass.OnResize(width, height);
    m_ShadingPass.OnResize(width, height);

    m_GBufferPass.GetTexture(Binding::T_DEPTH)
        ->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_GBufferPass.GetTexture(Binding::T_NORMAL)
        ->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_GBufferPass.GetTexture(Binding::T_MOTION_VECTOR)
        ->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_LightingPass.GetTexture(Binding::T_DIRECT_LIGHT)
        ->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_ShadingPass.GetTexture(Binding::T_SHADING)
        ->BindImGuiDescriptor(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    CreateDescriptorSets();
  }
}

void Renderer::RenderUI() {
  ImGui::Spacing();
  ImGui::Begin("Textures##Textures");

  VkCommandBuffer commandBuffer =
      Akari::Application::GetCommandBuffer({.Begin = true});

  uint32_t thumbSize = ImGui::GetContentRegionAvail().x;

  for (auto &image : m_DisplayImages) {
    float aspect = (float)image->GetWidth() / (float)image->GetHeight();
    float w = aspect > 1.0f ? thumbSize : thumbSize * aspect;
    float h = aspect > 1.0f ? thumbSize / aspect : thumbSize;
    const char *name = image->GetSpecification().ObjectName;
    if (name)
      ImGui::TextUnformatted(name);

    ImGui::Image(image->m_DescriptorSet, ImVec2(w, h));
  }

  Akari::Application::FlushCommandBuffer(commandBuffer);

  ImGui::End();
  ImGui::Spacing();
}

} // namespace Kitagawa