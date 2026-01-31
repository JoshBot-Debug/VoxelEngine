#pragma once
#include <glm/glm.hpp>

struct Voxel {
  uint32_t Material = 0;

  Voxel() = default;
  Voxel(uint32_t material);

  bool operator==(const Voxel& other) const;
  bool operator!=(const Voxel& other) const;
};

struct Voxel2 {
  uint32_t Id = 0;

  Voxel2() = default;
  Voxel2(uint32_t id);

  bool operator==(const Voxel2& other) const;
  bool operator!=(const Voxel2& other) const;
};