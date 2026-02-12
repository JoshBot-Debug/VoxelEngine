#include "Palette.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include "Debug.h"
#include "imgui.h"

const float CELL_SIZE    = 100.0f;
const float CELL_PADDING = 15.0f;
const float LABEL_HEIGHT = 20.0f;

void Palette::RenderPalette() {
  ImGui::Begin("Voxel Palette", nullptr, ImGuiWindowFlags_MenuBar);

  RenderFileMenu();

  float width = ImGui::GetContentRegionAvail().x;

  int columns = (int)((width + CELL_PADDING) / (CELL_SIZE + CELL_PADDING));
  if (columns < 1)
    columns = 1;

  for (size_t i = 0; i < m_Items.size(); i++) {
    Item& item = m_Items[i];
    ImGui::PushID(item.Id);

    ImGui::BeginGroup();

    if (ImGui::Selectable("##cell", m_SelectedItem == item.Id, ImGuiSelectableFlags_None, ImVec2(CELL_SIZE, CELL_SIZE))) {
      m_SelectedItem = item.Id;
    }

    ImVec2 pMin = ImGui::GetItemRectMin();
    ImVec2 pMax = ImGui::GetItemRectMax();
    ImU32  col =
        ImGui::GetColorU32(ImVec4{item.Mat->Albedo.r, item.Mat->Albedo.g, item.Mat->Albedo.b, item.Mat->Albedo.a});
    ImGui::GetWindowDrawList()->AddRectFilled(pMin, pMax, col);

    if (m_SelectedItem == item.Id) {
      const float MARGIN = 3.0f;
      ImGui::GetWindowDrawList()->AddRect(
          ImVec2(pMin.x - MARGIN, pMin.y - MARGIN),
          ImVec2(pMax.x + MARGIN, pMax.y + MARGIN),
          IM_COL32(255, 255, 0, 255),
          0,
          0,
          3.0f);
    }

    const char* text     = item.Name.c_str();
    ImVec2      textSize = ImGui::CalcTextSize(text);
    float       textX    = pMin.x + (CELL_SIZE - textSize.x) * 0.5f;
    float       textY    = pMax.y + 4.0f;
    ImGui::GetWindowDrawList()->AddText(ImVec2(textX, textY),
                                        IM_COL32(255, 255, 255, 255),
                                        text);

    ImGui::Dummy(ImVec2(CELL_SIZE, LABEL_HEIGHT + CELL_PADDING));

    ImGui::EndGroup();

    if ((i + 1) % columns != 0)
      ImGui::SameLine(0.0f, CELL_PADDING);

    ImGui::PopID();
  }

  ImGui::End();
}

void Palette::RenderEditor() {
  ImGui::Begin("Voxel Editor", nullptr, ImGuiWindowFlags_MenuBar);

  RenderFileMenu();

  float     width           = ImGui::GetContentRegionAvail().x;
  const int LABEL_WIDTH     = 100;
  int       MAX_INPUT_WIDTH = std::min(500, static_cast<int>(width) - LABEL_WIDTH);

  auto item = std::find_if(m_Items.begin(), m_Items.end(), [SelectedItem = m_SelectedItem](const Item& item) {
    return item.Id == SelectedItem;
  });

  auto Label = [MAX_INPUT_WIDTH](const std::string& label) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label.c_str());
    ImGui::SameLine(LABEL_WIDTH);
    ImGui::SetNextItemWidth(MAX_INPUT_WIDTH);
  };

  // Select/Delete a voxel
  {
    Label("Voxel");
    ImGui::SetNextItemWidth(MAX_INPUT_WIDTH - 80);
    if (ImGui::BeginCombo("##Voxel", item == m_Items.end() ? "None selected" : item->Name.c_str())) {
      for (int i = 0; i < m_Items.size(); i++) {
        const bool isSelected = (m_Items[i].Id == m_SelectedItem);

        if (ImGui::Selectable(m_Items[i].Name.c_str(), isSelected))
          m_SelectedItem = m_Items[i].Id;

        if (isSelected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("Delete", ImVec2(76, 0))) {
      if (item != m_Items.end()) {
        m_SelectedItem = 0;
        m_Items.erase(item);
        return ImGui::End();
      }
    }
  }

  if (item == m_Items.end())
    return ImGui::End();

  // Name
  {
    char bName[128];
    std::snprintf(bName, sizeof(bName), "%s", item->Name.c_str());
    Label("Name");
    if (ImGui::InputText("##Name", bName, sizeof(bName)))
      item->Name = bName;
  }

  // Albedo
  {
    Label("Albedo");
    ImGui::ColorEdit4("##Albedo", glm::value_ptr(item->Mat->Albedo), ImGuiColorEditFlags_Float);
  }

  // Emissive
  {
    Label("Emissive");
    ImGui::ColorEdit4("##Emissive", glm::value_ptr(item->Mat->Emissive), ImGuiColorEditFlags_Float);
  }

  // Metallic
  {
    Label("Metallic");
    ImGui::DragFloat("##Metallic", &item->Mat->Metallic, 0.001f, 0.0f, 1.0f, nullptr, ImGuiSliderFlags_AlwaysClamp);
  }

  // Roughness
  {
    Label("Roughness");
    ImGui::DragFloat("##Roughness", &item->Mat->Roughness, 0.001f, 0.0f, 1.0f, nullptr, ImGuiSliderFlags_AlwaysClamp);
  }

  ImGui::End();
}

void Palette::RenderFileMenu() {
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("Options")) {

      if (ImGui::MenuItem("+ New Voxel"))
        Create();

      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }
}

void Palette::RenderUI() {
  RenderPalette();
  RenderEditor();
}

void Palette::Create() {
  uint32_t nextId = static_cast<uint32_t>(m_Items.size()) + 1;
  m_Items.emplace_back(Item{
      .Id   = nextId,
      .Name = "Material #" + std::to_string(nextId),
      .Mat  = std::make_shared<Material>(nextId),
  });
}

void Palette::Create(Item item) {
  item.Id      = static_cast<uint32_t>(m_Items.size()) + 1;
  item.Mat->Id = item.Id;
  m_Items.emplace_back(item);
}

std::shared_ptr<Material> Palette::Find(const std::string& name) {
  auto item =
      std::find_if(m_Items.begin(), m_Items.end(), [name](const Item& item) { return item.Name == name; });

  if (item == m_Items.end())
    return nullptr;

  return item->Mat;
}

std::vector<Material> Palette::GetMaterials() {
  std::vector<Material> materials(m_Items.size());

  for (size_t i = 0; i < m_Items.size(); i++)
    materials[i] = *m_Items[i].Mat;

  return materials;
}

std::shared_ptr<Material> Palette::GetMaterial(uint32_t id) {
  for (size_t i = 0; i < m_Items.size(); i++)
    if (m_Items[i].Id == id)
      return m_Items[i].Mat;

  return nullptr;
}
