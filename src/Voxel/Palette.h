#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Type.h"
#include "Utility/Debug.h"
#include "Voxel.h"

class Palette {
public:
  struct Item {
    uint32_t                  Id = 0;
    std::string               Name;
    std::shared_ptr<Material> Mat;
  };

private:
  bool                  m_Dirty        = false;
  uint32_t              m_SelectedItem = 0;
  std::vector<Item>     m_Items        = {};
  std::function<void()> m_Flush        = nullptr;

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

  void OnFlush(std::function<void()> callback) { m_Flush = callback; };

  void Flush();

  void Clean();

  bool IsDirty() { return m_Dirty; };

  std::vector<Material> GetMaterials();

  std::shared_ptr<Material> GetMaterial(uint32_t id);
};