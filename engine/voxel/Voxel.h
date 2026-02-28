#pragma once
#include <glm/glm.hpp>

struct Voxel {
  uint32_t Id {0};

  Voxel() = default;
  Voxel(uint32_t id);

  bool operator==(const Voxel& other) const;
  bool operator!=(const Voxel& other) const;
};