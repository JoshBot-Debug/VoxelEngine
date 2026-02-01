#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "Camera/PerspectiveCamera.h"
#include "Voxel/Type.h"
#include "World.h"

namespace Kitagawa {

struct HighlightVertex {
  glm::vec3 Position;
  glm::vec3 Color;
};

class UI {
private:
  PerspectiveCamera* m_Camera = nullptr;
  World*             m_World  = nullptr;

  std::vector<HighlightVertex> m_HighlightVertices = {};

private:
  void ConstructHighlightVertices(const glm::vec3& voxelMin, const glm::vec3& hitNormal, const glm::vec3& color, float size);

public:
  void Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport);

  void SetCamera(PerspectiveCamera* camera) { m_Camera = camera; };
  void SetWorld(World* world) { m_World = world; };

  const std::vector<HighlightVertex>& GetHighlightVertices() { return m_HighlightVertices; };
};

} // namespace Kitagawa
