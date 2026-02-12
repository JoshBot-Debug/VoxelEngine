#pragma once

#include <glm/glm.hpp>

#include "camera/PerspectiveCamera.h"

namespace vxen {

class Controller {
private:
  glm::vec2 m_Mouse;
  bool      m_Focus;
  float     m_MovementSpeed = 30.0f;

public:
  Controller();

  void Update(float deltaTime, akari::camera::PerspectiveCamera& camera);

  void RenderUI(akari::camera::PerspectiveCamera& camera);

  void SetMovementSpeed(float speed) { m_MovementSpeed = speed; }

  void SetFocus(bool focus) { m_Focus = focus; }
};

} // namespace vxen
