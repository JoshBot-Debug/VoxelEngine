#pragma once

#include <glm/glm.hpp>
#include <iostream>

#include "KeyCodes.h"
#include "window/Application.h"

#include <stdio.h>

namespace akari::window {

class Input {
private:
  static glm::vec2 s_Scroll;

public:
  static const glm::vec2 MousePosition() {
    GLFWwindow* windowHandle = akari::window::Application::Get().GetWindowHandle();

    double x, y;
    glfwGetCursorPos(windowHandle, &x, &y);
    return glm::vec2(x, y);
  };

  static void ResetScroll() {
    s_Scroll.x = 0.0f;
    s_Scroll.y = 0.0f;
  }

  static void ScrollCallback(GLFWwindow* window, double x, double y) {
    (void)window;
    s_Scroll.x = static_cast<float>(x);
    s_Scroll.y = static_cast<float>(y);
  }

  static const glm::vec2 GetScroll() { return s_Scroll; }

  /**
   * @param key Expects a KeyboardKey or MouseButton
   */
  template <typename T>
  static bool KeyPress(const T& key) {
    GLFWwindow* windowHandle = Application::Get().GetWindowHandle();

    if (!windowHandle)
      return false;

    int state = 0;

    if constexpr (std::is_same_v<T, KeyboardKey>)
      state = glfwGetKey(windowHandle, static_cast<int>(key));

    else if constexpr (std::is_same_v<T, MouseButton>)
      state = glfwGetMouseButton(windowHandle, static_cast<int>(key));

    return state == static_cast<int>(KeyAction::PRESS);
  }

  /**
   * @param key Expects a KeyboardKey or MouseButton
   */
  template <typename T>
  static bool KeyRelease(const T& key) {
    GLFWwindow* windowHandle = Application::Get().GetWindowHandle();

    if (!windowHandle)
      return false;

    int state = 0;

    if constexpr (std::is_same_v<T, KeyboardKey>)
      state = glfwGetKey(windowHandle, static_cast<int>(key));

    else if constexpr (std::is_same_v<T, MouseButton>)
      state = glfwGetMouseButton(windowHandle, static_cast<int>(key));

    return state == static_cast<int>(KeyAction::RELEASE);
  }
};

inline glm::vec2 Input::s_Scroll(0.0f);

} // namespace akari::window
