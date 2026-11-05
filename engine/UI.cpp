#include "UI.h"
#include <imgui.h>

namespace vxen {

void UI::Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport) {
  if (ImGui::IsKeyPressed(ImGuiKey_LeftCtrl))
    m_MouseHit = m_World->Raycast(mouse);

  if (ImGui::IsKeyReleased(ImGuiKey_LeftCtrl))
    m_MouseHit.Reset();
}

} // namespace vxen
