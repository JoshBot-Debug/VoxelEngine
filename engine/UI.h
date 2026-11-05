#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "World.h"
#include "camera/PerspectiveCamera.h"
#include "voxel/Type.h"

namespace vxen {

struct HighlightVertex {
  glm::vec3 Position {glm::vec3(0.0f)};
  glm::vec3 Color {glm::vec3(0.0f)};
};

class UI {
private:
  akari::camera::PerspectiveCamera* m_Camera {nullptr};
  World*                            m_World {nullptr};

  SparseOctree::Hit m_MouseHit;

public:
  void Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport);

  void SetCamera(akari::camera::PerspectiveCamera* camera) { m_Camera = camera; };
  void SetWorld(World* world) { m_World = world; };

  const SparseOctree::Hit* GetMouseHit() { return &m_MouseHit; };
};

} // namespace vxen
