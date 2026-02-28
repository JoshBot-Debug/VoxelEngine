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

const int WORLD_SIZE {64};

using namespace akari::window;

class ViewportLayer : public Layer {

private:
  akari::camera::PerspectiveCamera m_Camera;
  vxen::Controller                 m_Controller;
  vxen::World                      m_World {WORLD_SIZE};
  vxen::UI                         m_UI;
  Scene                            m_Scene;

  glm::vec2 m_ViewportSize {1080.0f, 720.0f};
  glm::vec2 m_ViewportMouse {0.0f};

  float m_RenderTime {0.0f};
  float m_UpdateTime {0.0f};

public:
  bool ViewportLock = true;

  virtual void OnAttach() override {
    m_Camera.SetProjection(45.0f, 0.01f, 2048.0f);
    m_Camera.SetPosition(WORLD_SIZE / 2, WORLD_SIZE / 2, (WORLD_SIZE * 2) + (WORLD_SIZE / 4));
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
  }

  virtual void OnUpdate(float deltaTime) override {
    auto start = std::chrono::high_resolution_clock::now();

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

    auto                                     end      = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = end - start;
    m_UpdateTime                                      = duration.count();
  }

  virtual void OnRender() override {
    ImGui::Begin("Debug");

    ImGui::PushFont(Application::GetFont(Application::FontType::Font_Bold));
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

    const auto& flat = m_World.GetSVO();

    ImGui::Text("SVO Nodes: %d", (int)flat.size());
    ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("##svo_table", 5, flags)) {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableSetupColumn("Depth", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableSetupColumn("Children", ImGuiTableColumnFlags_WidthFixed, 110.0f);
      ImGui::TableSetupColumn("Child Index", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Child Id", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableHeadersRow();

      ImGuiListClipper clipper;
      clipper.Begin((int)flat.size());
      while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
          const auto& n = flat[i];

          ImGui::TableNextRow();

          ImGui::TableSetColumnIndex(0);
          ImGui::Text("%d", i);

          ImGui::TableSetColumnIndex(1);
          ImGui::Text("%u", (unsigned)n.GetDepth());

          ImGui::TableSetColumnIndex(2);
          ImGui::TextUnformatted(std::bitset<8>(n.GetChildren()).to_string().c_str());

          ImGui::TableSetColumnIndex(3);
          ImGui::Text("%u", (unsigned)n.ChildIndex);

          ImGui::TableSetColumnIndex(4);
          ImGui::Text("%u", (unsigned)n.GetId());
        }
      }

      ImGui::EndTable();
    }

    ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 {0.0f, 0.0f});
    ImGui::Begin("Viewport", nullptr, ViewportLock ? (ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse) : ImGuiWindowFlags_None);

    GetViewportInfo(m_ViewportMouse.x, m_ViewportMouse.y, m_ViewportSize.x, m_ViewportSize.y);

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

Application* akari::window::CreateApplication(int argc, char** argv) {

  const ApplicationSpecification applicationSpecification = {
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