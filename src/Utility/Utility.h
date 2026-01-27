#pragma once

#include <filesystem>
#include <limits.h>
#include <string>
#include <unistd.h>

#include <imgui.h>

inline std::string GetExecutableDir() {
  char    result[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
  return std::filesystem::path(
             std::string(result,
                         (count > 0) ? static_cast<unsigned int>(count) : 0))
      .parent_path()
      .string();
}

inline bool GetViewportMouse(float& x, float& y) {
  ImVec2 mouse     = ImGui::GetMousePos();
  ImVec2 windowPos = ImGui::GetWindowPos();

  ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
  ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

  ImVec2 viewportMin{windowPos.x + contentMin.x, windowPos.y + contentMin.y};
  ImVec2 viewportMax{windowPos.x + contentMax.x, windowPos.y + contentMax.y};

  if (mouse.x < viewportMin.x || mouse.x >= viewportMax.x ||
      mouse.y < viewportMin.y || mouse.y >= viewportMax.y) {
    return false;
  }

  float width  = viewportMax.x - viewportMin.x;
  float height = viewportMax.y - viewportMin.y;

  x = mouse.x - viewportMin.x;
  y = mouse.y - viewportMin.y;

  x = std::clamp(x, 0.0f, width - 1.0f);
  y = std::clamp(y, 0.0f, height - 1.0f);

  return true;
}

inline bool GetViewportInfo(float& mouseX, float& mouseY, float& viewportWidth, float& viewportHeight) {
  ImVec2 mouse     = ImGui::GetMousePos();
  ImVec2 windowPos = ImGui::GetWindowPos();

  ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
  ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

  ImVec2 viewportMin{windowPos.x + contentMin.x, windowPos.y + contentMin.y};
  ImVec2 viewportMax{windowPos.x + contentMax.x, windowPos.y + contentMax.y};

  if (mouse.x < viewportMin.x || mouse.x >= viewportMax.x ||
      mouse.y < viewportMin.y || mouse.y >= viewportMax.y) {
    return false;
  }

  viewportWidth  = viewportMax.x - viewportMin.x;
  viewportHeight = viewportMax.y - viewportMin.y;

  mouseX = mouse.x - viewportMin.x;
  mouseY = mouse.y - viewportMin.y;

  mouseX = std::clamp(mouseX, 0.0f, viewportWidth - 1.0f);
  mouseY = std::clamp(mouseY, 0.0f, viewportHeight - 1.0f);

  return true;
}