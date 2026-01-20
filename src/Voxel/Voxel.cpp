#include "Voxel.h"

Voxel::Voxel(uint32_t material) : Material(material) {}

bool Voxel::operator==(const Voxel &other) const {
  return Material == other.Material;
}

bool Voxel::operator!=(const Voxel &other) const { return !(*this == other); }