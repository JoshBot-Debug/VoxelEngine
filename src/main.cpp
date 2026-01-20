#include <chrono>
#include <iostream>

#include "Application.h"
#include "EntryPoint.h"
#include "Image.h"

#include "Utility/Debug.h"

#include "Kitagawa/Controller.h"
#include "Kitagawa/Render/PBR/Renderer.h"
#include "Kitagawa/Render/Pathtrace/Renderer.h"
#include "Kitagawa/Render/Raymarch/Renderer.h"
#include "Kitagawa/Render/Renderer.h"
#include "Kitagawa/World.h"

#include "Input/Input.h"

#include "Camera/PerspectiveCamera.h"

const int WORLD_SIZE = 256;

class ViewportLayer : public Akari::Layer {

private:
  PerspectiveCamera m_Camera;
  Kitagawa::Controller m_Controller;
  Kitagawa::World m_World{WORLD_SIZE};
  Kitagawa::Render::Renderer *m_Renderer = nullptr;

  uint32_t m_ViewportWidth = 1080;
  uint32_t m_ViewportHeight = 720;
  glm::vec2 m_ViewportMouse{0.0f};

  float m_RenderTime = 0.0f;
  float m_UpdateTime = 0.0f;

public:
  bool ViewportLock = true;

  ~ViewportLayer() { delete m_Renderer; }

  virtual void OnAttach() override {
    m_Camera.SetProjection(45.0f, 0.01f, 256.0f);
    m_Camera.SetPosition(WORLD_SIZE / 2, WORLD_SIZE / 2,
                         (WORLD_SIZE * 2) + (WORLD_SIZE / 4));
    m_Camera.SetRotation(0.0f, 0.0f, 0.0f);
    m_Controller.SetMovementSpeed(150.0f);
  }

  virtual void OnUpdate(float deltaTime) override {
    auto start = std::chrono::high_resolution_clock::now();

    m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);

    if (m_Renderer)
      m_Renderer->OnResize(m_ViewportWidth, m_ViewportHeight);

    m_Controller.Update(deltaTime, m_Camera);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;
    m_UpdateTime = duration.count();
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
    ImGui::Text("DDGI Probes: %i", m_World.GetSVO()->GetDDGIProbeSize());

    ImGui::Spacing();

    m_Controller.RenderUI(m_Camera);
    m_World.RenderUI();

    if (m_Renderer)
      m_Renderer->RenderUI();

    ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::Begin("Viewport", nullptr,
                 ViewportLock
                     ? (ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoCollapse)
                     : ImGuiWindowFlags_None);

    m_ViewportWidth = ImGui::GetContentRegionAvail().x;
    m_ViewportHeight = ImGui::GetContentRegionAvail().y;

    ImVec2 mouse = ImGui::GetMousePos();
    ImVec2 window = ImGui::GetWindowPos();

    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

    float contentHeight = contentMax.y - contentMin.y;

    float relativeY = ImGui::GetMousePos().y - (window.y + contentMin.y);

    m_ViewportMouse.x = mouse.x - window.x - contentMin.x;
    m_ViewportMouse.y = contentHeight - relativeY;

    m_Controller.SetFocus(ImGui::IsWindowFocused());

    auto start = std::chrono::high_resolution_clock::now();

    if (m_Renderer)
      m_Renderer->Render();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;
    m_RenderTime = duration.count();

    ImGui::End();
    ImGui::PopStyleVar(1);
  }

  void SetRenderer(Kitagawa::Render::Renderer *renderer) {
    delete m_Renderer;
    renderer->SetCamera(&m_Camera);
    renderer->SetWorld(&m_World);
    renderer->Initialize();
    renderer->OnResize(m_ViewportWidth, m_ViewportHeight);
    m_World.Flush();
    m_Renderer = std::move(renderer);
  }
};

Akari::Application *Akari::CreateApplication(int argc, char **argv) {

  const Akari::ApplicationSpecification applicationSpecification = {
      .Width = 1080,
      .Height = 720,
      .EnableDocking = true,
      .Maximized = true,
      .Centered = true,
  };

  Akari::Application *app = new Akari::Application(applicationSpecification);

  static int currentRenderer = -1;

  auto layer = std::make_shared<ViewportLayer>();

  app->SetMenubarCallback([layer]() {
    if (ImGui::BeginMenu("View ##MainMenu")) {

      ImGui::Checkbox("Lock Viewport ##ToggleViewportLock",
                      &layer->ViewportLock);

      if (ImGui::BeginMenu("Renderer")) {
        if (ImGui::RadioButton("Raymarch", currentRenderer == 0)) {
          currentRenderer = 0;
          layer->SetRenderer(reinterpret_cast<Kitagawa::Render::Renderer *>(
              new Kitagawa::Raymarch::Renderer()));
        }
        if (ImGui::RadioButton("Pathtrace", currentRenderer == 1)) {
          currentRenderer = 1;
          layer->SetRenderer(reinterpret_cast<Kitagawa::Render::Renderer *>(
              new Kitagawa::Pathtrace::Renderer()));
        }
        if (ImGui::RadioButton("PBR", currentRenderer == 2)) {
          currentRenderer = 2;
          layer->SetRenderer(reinterpret_cast<Kitagawa::Render::Renderer *>(
              new Kitagawa::PBR::Renderer()));
        }
        ImGui::EndMenu();
      }

      ImGui::EndMenu();
    }
  });

  app->PushLayer(layer);

  return app;
}