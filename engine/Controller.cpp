#include "Controller.h"

#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>

#include "window/Application.h"
#include "window/Input.h"
#include "Debug.h"

namespace vxen {

Controller::Controller() {
  GLFWwindow* windowHandle = akari::window::Application::Get().GetWindowHandle();
  glfwSetScrollCallback(windowHandle, akari::window::Input::ScrollCallback);
}

void Controller::Update(float deltaTime, akari::camera::PerspectiveCamera& camera) {

  if (m_Focus) {
    glm::vec3 translate(0.0f);
    glm::vec3 rotate(0.0f);

    const glm::vec2 scroll = akari::window::Input::GetScroll();

    if (scroll.y) {
      float direction = m_MovementSpeed * deltaTime * scroll.y;
      if (akari::window::Input::KeyPress(akari::window::KeyboardKey::LEFT_SHIFT))
        translate.x += direction;
      else
        translate.z += direction;
    }

    if (scroll.x)
      translate.x += (m_MovementSpeed * deltaTime) * scroll.x * 50.0f;

    if (akari::window::Input::KeyPress(akari::window::KeyboardKey::W))
      translate.z += m_MovementSpeed * deltaTime;

    if (akari::window::Input::KeyPress(akari::window::KeyboardKey::S))
      translate.z -= m_MovementSpeed * deltaTime;

    if (akari::window::Input::KeyPress(akari::window::KeyboardKey::A))
      translate.x -= m_MovementSpeed * deltaTime;

    if (akari::window::Input::KeyPress(akari::window::KeyboardKey::D))
      translate.x += m_MovementSpeed * deltaTime;

    if (akari::window::Input::KeyPress(akari::window::KeyboardKey::E))
      translate.y += m_MovementSpeed * deltaTime;

    if (akari::window::Input::KeyPress(akari::window::KeyboardKey::Q))
      translate.y -= m_MovementSpeed * deltaTime;

    if (akari::window::Input::KeyPress(akari::window::MouseButton::LEFT)) {
      glm::vec2 dm = glm::mix(glm::vec2(0.0f),
                              akari::window::Input::MousePosition() - m_Mouse, 0.1f);
      camera.Rotate(-dm.y, dm.x, 0.0f);
    }

    camera.Translate(translate.x, translate.y, translate.z);

    camera.Update();
  }

  m_Mouse = akari::window::Input::MousePosition();
}

void Controller::RenderUI(akari::camera::PerspectiveCamera& camera) {
  ImGui::Spacing();

  if (ImGui::CollapsingHeader("Camera ##CameraMenu")) {

    ImGui::Spacing();
    ImGui::DragFloat3("Position ##CameraPosition",
                      glm::value_ptr(camera.Position));
    ImGui::Spacing();
    ImGui::DragFloat3("Rotation ##CameraRotation",
                      glm::value_ptr(camera.Rotation));
    ImGui::Spacing();
    ImGui::InputFloat("Near Plane ##CameraNearPlane", &camera.NearPlane);
    ImGui::Spacing();
    ImGui::InputFloat("Far Plane ##CameraFarPlane", &camera.FarPlane);
    ImGui::Spacing();
    ImGui::InputFloat("FOV ##CameraFOV", &camera.FOV);
    ImGui::Spacing();
    ImGui::InputFloat("Movement Speed ##MovementSpeed", &m_MovementSpeed);
  }
}

} // namespace vxen