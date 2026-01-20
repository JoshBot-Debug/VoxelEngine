#include "Controller.h"

#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>

#include "Application.h"
#include "Input/Input.h"
#include "Utility/Debug.h"

namespace Kitagawa {

Controller::Controller() {
  GLFWwindow *windowHandle = Akari::Application::Get().GetWindowHandle();
  glfwSetScrollCallback(windowHandle, Akari::Input::ScrollCallback);
}

void Controller::Update(float deltaTime, PerspectiveCamera &camera) {

  if (m_Focus) {
    glm::vec3 translate(0.0f);
    glm::vec3 rotate(0.0f);

    const glm::vec2 scroll = Akari::Input::GetScroll();

    if (scroll.y) {
      float direction = m_MovementSpeed * deltaTime * scroll.y;
      if (Akari::Input::KeyPress(Akari::KeyboardKey::LEFT_SHIFT))
        translate.x += direction;
      else
        translate.z += direction;
    }

    if (scroll.x)
      translate.x += (m_MovementSpeed * deltaTime) * scroll.x * 50.0f;

    if (Akari::Input::KeyPress(Akari::KeyboardKey::W))
      translate.z += m_MovementSpeed * deltaTime;

    if (Akari::Input::KeyPress(Akari::KeyboardKey::S))
      translate.z -= m_MovementSpeed * deltaTime;

    if (Akari::Input::KeyPress(Akari::KeyboardKey::A))
      translate.x -= m_MovementSpeed * deltaTime;

    if (Akari::Input::KeyPress(Akari::KeyboardKey::D))
      translate.x += m_MovementSpeed * deltaTime;

    if (Akari::Input::KeyPress(Akari::KeyboardKey::E))
      translate.y += m_MovementSpeed * deltaTime;

    if (Akari::Input::KeyPress(Akari::KeyboardKey::Q))
      translate.y -= m_MovementSpeed * deltaTime;

    if (Akari::Input::KeyPress(Akari::MouseButton::LEFT)) {
      glm::vec2 dm = glm::mix(glm::vec2(0.0f),
                              Akari::Input::MousePosition() - m_Mouse, 0.1f);
      camera.Rotate(-dm.y, dm.x, 0.0f);
    }

    camera.Translate(translate.x, translate.y, translate.z);

    camera.Update();
  }

  m_Mouse = Akari::Input::MousePosition();
}

void Controller::RenderUI(PerspectiveCamera &camera) {
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

} // namespace Kitagawa