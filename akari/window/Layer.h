#pragma once

namespace akari::window {

class Layer {
private:
public:
  virtual ~Layer() = default;

  virtual void OnAttach() {}
  virtual void OnDetach() {}

  virtual void OnUpdate(float deltaTime) {}
  virtual void OnRender() {}
};

} // namespace akari::window