#include "Voxel.h"

Voxel::Voxel(uint32_t material) : Material(material) {}

bool Voxel::operator==(const Voxel& other) const {
  return Material == other.Material;
}

bool Voxel::operator!=(const Voxel& other) const { return !(*this == other); }

Voxel2::Voxel2(uint32_t id) : Id(id) {}

bool Voxel2::operator==(const Voxel2& other) const {
  return Id == other.Id;
}

bool Voxel2::operator!=(const Voxel2& other) const { return Id != other.Id; }