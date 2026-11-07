#include "UI.h"
#include <imgui.h>

namespace vxen {

void UI::Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport) {
  if (ImGui::IsKeyPressed(ImGuiKey_LeftCtrl)) {
    auto hit = m_World->Raycast(mouse);
    if (hit.Id)
      m_MouseHit = hit;
  }

  if (ImGui::IsKeyReleased(ImGuiKey_LeftCtrl)) {
    m_MouseHit.Reset();
    m_PushConstants.Reset();
  }

  m_PushConstants.Position = m_MouseHit.Position;
  m_PushConstants.Normal   = m_MouseHit.Normal;
  m_PushConstants.Size     = m_MouseHit.Size;
  m_PushConstants.Size     = m_MouseHit.Size;
}

} // namespace vxen
