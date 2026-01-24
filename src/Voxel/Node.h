#pragma once
#include "Voxel/Voxel.h"
#include <glm/glm.hpp>

using VoxelType = Voxel;

struct Node {
  uint8_t    Depth       = 0;
  VoxelType* Voxel       = nullptr;
  Node*      Children[8] = {nullptr, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr, nullptr};
  bool       IsCulled    = false;

  Node();
  Node(uint8_t depth, VoxelType* voxel = nullptr);
  ~Node();

  bool operator==(const Node& other) const;
  bool operator!=(const Node& other) const;

  void Clear();
};
