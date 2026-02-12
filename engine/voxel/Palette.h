#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Type.h"
#include "Debug.h"
#include "Voxel.h"

class Palette {
public:
  struct Item {
    uint32_t                  Id = 0;
    std::string               Name;
    std::shared_ptr<Material> Mat;
  };

private:
  uint32_t              m_SelectedItem = 0;
  std::vector<Item>     m_Items        = {};

private:
  void RenderPalette();
  void RenderEditor();
  void RenderFileMenu();

public:
  Palette()  = default;
  ~Palette() = default;

  void RenderUI();

  void Create();

  void Create(Item item);

  std::shared_ptr<Material> Find(const std::string& name);

  std::vector<Material> GetMaterials();

  std::shared_ptr<Material> GetMaterial(uint32_t id);
};