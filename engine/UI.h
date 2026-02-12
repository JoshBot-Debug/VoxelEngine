#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "camera/PerspectiveCamera.h"
#include "voxel/Type.h"
#include "World.h"

namespace vxen {

struct HighlightVertex {
  glm::vec3 Position;
  glm::vec3 Color;
};

class UI {
private:
  akari::camera::PerspectiveCamera* m_Camera = nullptr;
  World*             m_World  = nullptr;

  std::vector<HighlightVertex> m_HighlightVertices = {};

private:
  void ConstructHighlightVertices(const glm::vec3& voxelMin, const glm::vec3& hitNormal, const glm::vec3& color, float size);

public:
  void Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport);

  void SetCamera(akari::camera::PerspectiveCamera* camera) { m_Camera = camera; };
  void SetWorld(World* world) { m_World = world; };

  const std::vector<HighlightVertex>& GetHighlightVertices() { return m_HighlightVertices; };
};

} // namespace vxen
