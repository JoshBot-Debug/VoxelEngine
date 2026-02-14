#pragma once

#include "voxel/SparseOctree.h"
#include "voxel/Type.h"
#include "voxel/Voxel.h"

template <uint32_t SS>
class Chunk {
private:
  SparseOctree<Voxel, SS>* m_SVO = nullptr;

  static uint32_t UID() {
    static uint32_t uid = 1;
    return uid++;
  }

public:
  uint32_t Id = 0;

public:
  Chunk()
      : m_SVO(new SparseOctree<Voxel>())
      , Id(UID()){};
  ~Chunk() { delete m_SVO; };
};