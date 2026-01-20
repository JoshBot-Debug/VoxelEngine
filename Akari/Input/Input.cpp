#include "Input.h"

namespace Akari {

const glm::vec2 Input::MousePosition() {
  GLFWwindow *windowHandle = Application::Get().GetWindowHandle();

  double x, y;
  glfwGetCursorPos(windowHandle, &x, &y);
  return glm::vec2(x, y);
}

} // namespace Akari
