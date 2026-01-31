#include "Voxel.h"

Voxel::Voxel(uint32_t id) : Id(id) {}

bool Voxel::operator==(const Voxel& other) const {
  return Id == other.Id;
}

bool Voxel::operator!=(const Voxel& other) const { return Id != other.Id; }