#include "UI.h"

#include "ChunkManager.h"
#include <imgui.h>

namespace Kitagawa {

void UI::ConstructHighlightVertices(const glm::vec3& voxelMin, const glm::vec3& hitNormal, const glm::vec3& color, float size) {

  glm::vec3 min = voxelMin;
  glm::vec3 max = voxelMin + glm::vec3(size);

  glm::vec3 v0, v1, v2, v3;

  // Determine face from normal
  if (hitNormal.x > 0.5f) { // +X
    v0 = {max.x, min.y, min.z};
    v1 = {max.x, max.y, min.z};
    v2 = {max.x, max.y, max.z};
    v3 = {max.x, min.y, max.z};
  } else if (hitNormal.x < -0.5f) { // -X
    v0 = {min.x, min.y, max.z};
    v1 = {min.x, max.y, max.z};
    v2 = {min.x, max.y, min.z};
    v3 = {min.x, min.y, min.z};
  } else if (hitNormal.y > 0.5f) { // +Y
    v0 = {min.x, max.y, min.z};
    v1 = {max.x, max.y, min.z};
    v2 = {max.x, max.y, max.z};
    v3 = {min.x, max.y, max.z};
  } else if (hitNormal.y < -0.5f) { // -Y
    v0 = {min.x, min.y, max.z};
    v1 = {max.x, min.y, max.z};
    v2 = {max.x, min.y, min.z};
    v3 = {min.x, min.y, min.z};
  } else if (hitNormal.z > 0.5f) { // +Z
    v0 = {min.x, min.y, max.z};
    v1 = {max.x, min.y, max.z};
    v2 = {max.x, max.y, max.z};
    v3 = {min.x, max.y, max.z};
  } else { // -Z
    v0 = {min.x, max.y, min.z};
    v1 = {max.x, max.y, min.z};
    v2 = {max.x, min.y, min.z};
    v3 = {min.x, min.y, min.z};
  }

  m_HighlightVertices = {{v0, color},
                         {v1, color},
                         {v2, color},

                         {v0, color},
                         {v2, color},
                         {v3, color}};
}

void UI::Update(double delta, const glm::vec2& mouse, const glm::vec2& viewport) {
  glm::vec3 rayOrigin    = m_Camera->Position;
  glm::vec3 rayDirection = m_Camera->GetRayDirection(mouse.x, mouse.y);

  bool isCtrlPressed = ImGui::IsKeyPressed(ImGuiKey_LeftCtrl);

  if (isCtrlPressed) {
    ChunkManager*            chunkManager = m_World->GetChunkManager();
    SparseOctree<Voxel>::Hit hit          = chunkManager->DeepRaymarch(chunkManager->GetChunkCoord(rayOrigin), rayOrigin, rayDirection);

    ConstructHighlightVertices(hit.Position, hit.Normal, glm::vec3(1.0f, 0.0f, 0.0f), hit.Size);
  }
}

} // namespace Kitagawa
