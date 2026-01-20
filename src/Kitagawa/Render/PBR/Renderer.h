#pragma once

#include "Camera/PerspectiveCamera.h"

#include "Kitagawa/Render/CameraBuffer.h"
#include "Kitagawa/Render/Renderer.h"
#include "Kitagawa/World.h"

#include "Kitagawa/Render/RenderPass/AmbientOcclusionPass.h"
#include "Kitagawa/Render/RenderPass/GBufferPass.h"
#include "Kitagawa/Render/RenderPass/LightingPass.h"
#include "Kitagawa/Render/RenderPass/ReflectionPass.h"
#include "Kitagawa/Render/RenderPass/ShadingPass.h"
#include "Kitagawa/Render/RenderPass/ShadowPass.h"

using namespace Kitagawa::Render;

namespace Kitagawa {
namespace PBR {

class Renderer : public Kitagawa::Render::Renderer {
private:
  World *m_World;
  PerspectiveCamera *m_Camera;
  RenderPass::GBufferPass m_GBufferPass;
  RenderPass::LightingPass m_LightingPass;
  RenderPass::ShadowPass m_ShadowPass;
  RenderPass::AmbientOcclusionPass m_AmbientOcclusionPass;
  RenderPass::ReflectionPass m_ReflectionPass;
  RenderPass::ShadingPass m_ShadingPass;

  Buffer m_SVOBuffer;
  Buffer m_LightBuffer;
  Buffer m_MaterialBuffer;
  Buffer m_MaterialLUTBuffer;
  Buffer m_VertexBuffer{{.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT}};

  CameraBuffer m_CameraBuffer;

  VkDevice m_Device = VK_NULL_HANDLE;
  VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

  std::vector<std::shared_ptr<Akari::Image>> m_DisplayImages = {};

private:
  void CreateDescripterPool(const std::vector<VkDescriptorPoolSize> &pool);
  void CreateDescriptorSets();

public:
  Renderer();
  ~Renderer();

  void Initialize() override;

  void Render() override;

  void OnResize(uint32_t width, uint32_t height) override;

  void SetCamera(PerspectiveCamera *camera) override { m_Camera = camera; };

  void SetWorld(World *world) override { m_World = world; };

  void RenderUI() override;
};

} // namespace PBR
} // namespace Kitagawa