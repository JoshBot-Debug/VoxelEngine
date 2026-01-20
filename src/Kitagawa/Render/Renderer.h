#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "Camera/PerspectiveCamera.h"
#include "Image.h"
#include "Kitagawa/World.h"

namespace Kitagawa {
namespace Render {
class Renderer {
public:
  virtual ~Renderer() = default;

  virtual void Initialize() = 0;

  virtual void Render() = 0;

  virtual void OnResize(uint32_t width, uint32_t height) = 0;

  virtual void SetCamera(PerspectiveCamera *camera) = 0;

  virtual void SetWorld(World *world) = 0;

  virtual void RenderUI() = 0;
};
} // namespace Render
} // namespace Kitagawa