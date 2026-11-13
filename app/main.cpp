#include <bitset>
#include <chrono>
#include <iostream>

#include "window/Application.h"
#include "window/EntryPoint.h"
#include "window/Image.h"

#include "Debug.h"
#include "Utility.h"

#include "Controller.h"
#include "UI.h"
#include "World.h"

#include "window/Input.h"

#include "Scene.h"
#include "camera/PerspectiveCamera.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

using namespace akari::window;

constexpr uint32_t MAX_FRAME_RATE_CAPTURE = 120;

auto getAverage = [](const float* values, uint32_t count) {
  if (count == 0)
    return 0.0f;

  float sum = 0.0f;

  for (uint32_t i = 0; i < count; i++)
    sum += values[i];

  return sum / count;
};

class ViewportLayer : public Layer {

private:
  akari::camera::PerspectiveCamera m_Camera;
  vxen::Controller                 m_Controller;
  vxen::World                      m_World;
  vxen::UI                         m_UI;
  Scene                            m_Scene;

  glm::vec2 m_ViewportSize {1080.0f, 720.0f};
  glm::vec2 m_ViewportMouse {0.0f};

  uint32_t rFrame      = 0;
  uint32_t rFrameCount = 0;
  float    m_RenderTime[MAX_FRAME_RATE_CAPTURE] {};

  uint32_t uFrame      = 0;
  uint32_t uFrameCount = 0;
  float    m_UpdateTime[MAX_FRAME_RATE_CAPTURE] {};

public:
  bool ViewportLock = true;

  virtual void OnAttach() override {
    m_Camera.SetProjection(45.0f, 0.01f, 2048.0f);
    m_Camera.SetPosition(32, 32, 144);
    m_Camera.SetRotation(0.0f, 0.0f, 0.0f);
    m_Controller.SetMovementSpeed(150.0f);

    m_UI.SetWorld(&m_World);
    m_UI.SetCamera(&m_Camera);

    m_Scene.SetUI(&m_UI);
    m_Scene.SetWorld(&m_World);
    m_Scene.SetCamera(&m_Camera);
    m_Scene.Initialize({
        .PolygonMode = VK_POLYGON_MODE_FILL,
        .Width       = static_cast<uint32_t>(m_ViewportSize.x),
        .Height      = static_cast<uint32_t>(m_ViewportSize.y),
    });

    m_World.SetCamera(&m_Camera);
    m_World.SetUI(&m_UI);
    m_World.SetMouseHit(m_UI.GetMouseHit());
  }

  virtual void OnUpdate(float deltaTime) override {
    auto start = std::chrono::steady_clock::now();

    m_Camera.OnResize(m_ViewportSize.x, m_ViewportSize.y);

    m_Scene.OnResize(m_ViewportSize.x, m_ViewportSize.y);

    m_Controller.Update(deltaTime, m_Camera);

    m_UI.Update(deltaTime, m_ViewportMouse, m_ViewportSize);
    m_World.Update(deltaTime, m_ViewportMouse, m_ViewportSize);

    if (ImGui::IsKeyPressed(ImGuiKey_L)) {
      m_Scene.Initialize({
          .PolygonMode = VK_POLYGON_MODE_LINE,
          .Width       = static_cast<uint32_t>(m_ViewportSize.x),
          .Height      = static_cast<uint32_t>(m_ViewportSize.y),
      });
    }

    if (ImGui::IsKeyPressed(ImGuiKey_P)) {
      m_Scene.Initialize({
          .PolygonMode = VK_POLYGON_MODE_FILL,
          .Width       = static_cast<uint32_t>(m_ViewportSize.x),
          .Height      = static_cast<uint32_t>(m_ViewportSize.y),
      });
    }

    auto                                     end      = std::chrono::steady_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;
    m_UpdateTime[uFrame]                              = duration.count();
    uFrame                                            = (uFrame + 1) % MAX_FRAME_RATE_CAPTURE;
    uFrameCount                                       = std::min(uFrameCount + 1, MAX_FRAME_RATE_CAPTURE);
  }

  virtual void OnRender() override {
    ImGui::Begin("Debug");

    ImGui::PushFont(Application::GetFont(Application::FontType::Font_Bold));
    ImGui::Text("General");
    ImGui::PopFont();
    ImGui::Separator();

    const float averageRenderTime = getAverage(m_RenderTime, rFrameCount);
    const float averageFPS        = averageRenderTime > 0.0f ? 1000.0f / averageRenderTime : 0.0f;

    ImGui::Text("Update Time: %.3f", m_UpdateTime[rFrame]);
    ImGui::Text("Render Time: %.3f", m_RenderTime[rFrame]);
    ImGui::Text("FPS: %.1f", averageFPS);
    ImGui::Text("Mouse Position %f, %f", m_ViewportMouse.x, m_ViewportMouse.y);

    {
      const auto hit = m_UI.GetMouseHit();

      ImGui::Text("Mouse Hit");

      ImGui::Text("ID: %u", hit->Id);
      ImGui::Text("Chunk: %u, %u, %u",
                  static_cast<uint32_t>(hit->Position.x / 64u),
                  static_cast<uint32_t>(hit->Position.y / 64u),
                  static_cast<uint32_t>(hit->Position.z / 64u));
      ImGui::Text("Position: %.3f, %.3f, %.3f",
                  hit->Position.x,
                  hit->Position.y,
                  hit->Position.z);

      ImGui::Text("Normal: %.3f, %.3f, %.3f",
                  hit->Normal.x,
                  hit->Normal.y,
                  hit->Normal.z);
      ImGui::Text("Size: %u", hit->Size);
    }

    ImGui::Spacing();

    m_Controller.RenderUI(m_Camera);
    m_World.RenderUI();
    m_Scene.RenderUI();

    ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 {0.0f, 0.0f});
    ImGui::Begin("Viewport", nullptr, ViewportLock ? (ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse) : ImGuiWindowFlags_None);

    GetViewportInfo(m_ViewportMouse.x, m_ViewportMouse.y, m_ViewportSize.x, m_ViewportSize.y);

    m_Controller.SetFocus(ImGui::IsWindowFocused());

    auto start = std::chrono::steady_clock::now();

    m_Scene.Render();

    auto                                     end      = std::chrono::steady_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;
    m_RenderTime[rFrame]                              = duration.count();
    rFrame                                            = (rFrame + 1) % MAX_FRAME_RATE_CAPTURE;
    rFrameCount                                       = std::min(rFrameCount + 1, MAX_FRAME_RATE_CAPTURE);

    ImGui::End();
    ImGui::PopStyleVar(1);
  }
};

Application* akari::window::CreateApplication(int argc, char** argv) {

  const ApplicationSpecification applicationSpecification {
      .Width {1440},
      .Height {300},
      .EnableDocking {true},
      .Maximized {true},
      .Centered {true},
  };

  Application* app = new Application(applicationSpecification);

  app->PushLayer<ViewportLayer>();

  return app;
}