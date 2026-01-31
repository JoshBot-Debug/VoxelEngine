#include <chrono>
#include <iostream>

#include "Application.h"
#include "EntryPoint.h"
#include "Image.h"

#include "Utility/Debug.h"
#include "Utility/Utility.h"

#include "Kitagawa/Controller.h"
#include "Kitagawa/World.h"

#include "Input/Input.h"

#include "Camera/PerspectiveCamera.h"
#include "Kitagawa/Scene.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

const int WORLD_SIZE = 64;

class ViewportLayer : public Akari::Layer {

private:
  PerspectiveCamera    m_Camera;
  Kitagawa::Controller m_Controller;
  Kitagawa::World      m_World{WORLD_SIZE};
  Scene                m_Scene;

  uint32_t  m_ViewportWidth  = 1080;
  uint32_t  m_ViewportHeight = 720;
  glm::vec2 m_ViewportMouse{0.0f};

  float m_RenderTime = 0.0f;
  float m_UpdateTime = 0.0f;

public:
  bool ViewportLock = true;

  virtual void OnAttach() override {
    m_Camera.SetProjection(45.0f, 0.01f, 2048.0f);
    m_Camera.SetPosition(WORLD_SIZE / 2, WORLD_SIZE / 2, (WORLD_SIZE * 2) + (WORLD_SIZE / 4));
    m_Camera.SetRotation(0.0f, 0.0f, 0.0f);
    m_Controller.SetMovementSpeed(150.0f);

    m_Scene.SetCamera(&m_Camera);
    m_Scene.SetWorld(&m_World);
    m_Scene.Initialize({
        .width  = m_ViewportWidth,
        .height = m_ViewportHeight,
    });

    m_World.Flush();
  }

  virtual void OnUpdate(float deltaTime) override {
    auto start = std::chrono::high_resolution_clock::now();

    m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);

    m_Scene.OnResize(m_ViewportWidth, m_ViewportHeight);

    m_Controller.Update(deltaTime, m_Camera);

    auto                                     end      = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;
    m_UpdateTime                                      = duration.count();
  }

  virtual void OnRender() override {
    ImGui::Begin("Debug");

    ImGui::PushFont(
        Akari::Application::GetFont(Akari::Application::FontType::Font_Bold));
    ImGui::Text("General");
    ImGui::PopFont();
    ImGui::Separator();

    ImGui::Text("Update Time: %f", m_UpdateTime);
    ImGui::Text("Render Time: %f", m_RenderTime);
    ImGui::Text("FPS: %f", 1000 / m_RenderTime);
    ImGui::Text("Mouse Position %f, %f", m_ViewportMouse.x, m_ViewportMouse.y);

    ImGui::Spacing();

    m_Controller.RenderUI(m_Camera);
    m_World.RenderUI();
    m_Scene.RenderUI();

    ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::Begin("Viewport", nullptr, ViewportLock ? (ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse) : ImGuiWindowFlags_None);

    m_ViewportWidth  = ImGui::GetContentRegionAvail().x;
    m_ViewportHeight = ImGui::GetContentRegionAvail().y;

    GetViewportMouse(m_ViewportMouse.x, m_ViewportMouse.y);

    m_Controller.SetFocus(ImGui::IsWindowFocused());

    auto start = std::chrono::high_resolution_clock::now();

    m_Scene.Render();

    auto                                     end      = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;
    m_RenderTime                                      = duration.count();

    ImGui::End();
    ImGui::PopStyleVar(1);
  }
};

Akari::Application* Akari::CreateApplication(int argc, char** argv) {

  const Akari::ApplicationSpecification applicationSpecification = {
      .Width         = 1440,
      .Height        = 300,
      .EnableDocking = true,
      .Maximized     = true,
      .Centered      = true,
  };

  Akari::Application* app = new Akari::Application(applicationSpecification);

  app->PushLayer<ViewportLayer>();

  return app;
}