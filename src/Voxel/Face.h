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
  static void Top(std::vector<Vertex>& vertices, float px, float py, float pz,
                  float sx, float sy, float sz, uint32_t material);
  static void Bottom(std::vector<Vertex>& vertices, float px, float py,
                     float pz, float sx, float sy, float sz, uint32_t material);
  static void Left(std::vector<Vertex>& vertices, float px, float py, float pz,
                   float sx, float sy, float sz, uint32_t material);
  static void Right(std::vector<Vertex>& vertices, float px, float py, float pz,
                    float sx, float sy, float sz, uint32_t material);
  static void Front(std::vector<Vertex>& vertices, float px, float py, float pz,
                    float sx, float sy, float sz, uint32_t material);
  static void Back(std::vector<Vertex>& vertices, float px, float py, float pz,
                   float sx, float sy, float sz, uint32_t material);
};