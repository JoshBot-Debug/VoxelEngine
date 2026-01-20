#pragma once
#include <glm/glm.hpp>

struct Voxel {
  uint32_t Material = 0;

  Voxel() = default;
  Voxel(uint32_t material);

  bool operator==(const Voxel &other) const;
  bool operator!=(const Voxel &other) const;
};