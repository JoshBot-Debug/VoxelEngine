#pragma once

#include <vector>

#include "Type.h"
#include "Voxel.h"

enum class FaceType : uint8_t { TOP,
                                BOTTOM,
                                LEFT,
                                RIGHT,
                                FRONT,
                                BACK };

class Face {
public:
  static void Top(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t id);
  static void Bottom(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t id);
  static void Left(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t id);
  static void Right(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t id);
  static void Front(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t id);
  static void Back(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t id);
};