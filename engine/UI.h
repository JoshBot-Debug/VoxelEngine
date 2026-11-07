#pragma once

#include <glm/glm.hpp>

#include "World.h"
#include "camera/PerspectiveCamera.h"
#include "voxel/Type.h"
#include <array>

namespace vxen {

class UI {
public:
  struct PushContants {
    glm::vec4 Position;
    glm::vec4 Normal;
    uint32_t  Size;
    uint32_t  Padding[3];

    void Reset() {
      Position = glm::vec4(0.0f);
      Normal   = glm::vec4(0.0f);
      Size     = 0;
    }
  };

private:
  akari::camera::PerspectiveCamera* m_Camera {nullptr};
  World*                            m_World {nullptr};

  SparseOctree::Hit m_MouseHit;

  PushContants m_PushConstants;

public:
  void Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport);

  void SetCamera(akari::camera::PerspectiveCamera* camera) { m_Camera = camera; };
  void SetWorld(World* world) { m_World = world; };

  PushContants&            GetPushContants() { return m_PushConstants; };
  const SparseOctree::Hit* GetMouseHit() { return &m_MouseHit; };
};

} // namespace vxen
