#pragma once

#include <glm/glm.hpp>
#include <vector>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Camera/PerspectiveCamera.h"

namespace Kitagawa {
namespace Render {

class CameraBuffer {
public:
  struct alignas(16) Camera {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection;
    glm::mat4 inverseView;
    glm::mat4 inverseProjection;
    glm::mat4 inverseViewProjection;
    glm::mat4 pView;
    glm::mat4 pProjection;
    glm::mat4 pViewProjection;
    glm::mat4 pInverseView;
    glm::mat4 pInverseProjection;
    glm::mat4 pInverseViewProjection;
    glm::vec3 position;
    float farPlane;
  };

private:
  std::vector<void *> m_Mapped;
  std::vector<VkBuffer> m_Buffers;
  std::vector<VmaAllocation> m_Allocations;

public:
  CameraBuffer();
  ~CameraBuffer();

  VkBuffer GetBuffer(uint32_t frame) { return m_Buffers[frame]; };

  void Render(PerspectiveCamera *camera);
};

} // namespace Render

} // namespace Kitagawa
