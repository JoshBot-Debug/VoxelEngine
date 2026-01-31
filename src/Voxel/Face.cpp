#include "Face.h"

void Face::Top(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t material) {
  vertices.emplace_back(Vertex{px, py + sy, pz, 0, 1, 0, material});
  vertices.emplace_back(
      Vertex{px + sx, py + sy, pz + sz, 0, 1, 0, material});
  vertices.emplace_back(Vertex{px + sx, py + sy, pz, 0, 1, 0, material});

  vertices.emplace_back(Vertex{px, py + sy, pz, 0, 1, 0, material});
  vertices.emplace_back(Vertex{px, py + sy, pz + sz, 0, 1, 0, material});
  vertices.emplace_back(
      Vertex{px + sx, py + sy, pz + sz, 0, 1, 0, material});
}

void Face::Bottom(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t material) {
  vertices.emplace_back(Vertex{px, py, pz, 0, -1, 0, material});
  vertices.emplace_back(Vertex{px + sx, py, pz, 0, -1, 0, material});
  vertices.emplace_back(
      Vertex{px + sx, py, pz + sz, 0, -1, 0, material});

  vertices.emplace_back(Vertex{px, py, pz, 0, -1, 0, material});
  vertices.emplace_back(
      Vertex{px + sx, py, pz + sz, 0, -1, 0, material});
  vertices.emplace_back(Vertex{px, py, pz + sz, 0, -1, 0, material});
}

void Face::Front(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t material) {
  vertices.emplace_back(Vertex{px, py, pz, 0, 0, -1, material});
  vertices.emplace_back(
      Vertex{px + sx, py + sy, pz, 0, 0, -1, material});
  vertices.emplace_back(Vertex{px + sx, py, pz, 0, 0, -1, material});

  vertices.emplace_back(Vertex{px, py, pz, 0, 0, -1, material});
  vertices.emplace_back(Vertex{px, py + sy, pz, 0, 0, -1, material});
  vertices.emplace_back(
      Vertex{px + sx, py + sy, pz, 0, 0, -1, material});
}

void Face::Back(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t material) {
  vertices.emplace_back(Vertex{px, py, pz + sz, 0, 0, 1, material});
  vertices.emplace_back(Vertex{px + sx, py, pz + sz, 0, 0, 1, material});
  vertices.emplace_back(
      Vertex{px + sx, py + sy, pz + sz, 0, 0, 1, material});

  vertices.emplace_back(Vertex{px, py, pz + sz, 0, 0, 1, material});
  vertices.emplace_back(
      Vertex{px + sx, py + sy, pz + sz, 0, 0, 1, material});
  vertices.emplace_back(Vertex{px, py + sy, pz + sz, 0, 0, 1, material});
}
void Face::Left(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t material) {
  vertices.emplace_back(Vertex{px, py, pz, -1, 0, 0, material});
  vertices.emplace_back(Vertex{px, py, pz + sz, -1, 0, 0, material});
  vertices.emplace_back(
      Vertex{px, py + sy, pz + sz, -1, 0, 0, material});

  vertices.emplace_back(Vertex{px, py, pz, -1, 0, 0, material});
  vertices.emplace_back(
      Vertex{px, py + sy, pz + sz, -1, 0, 0, material});
  vertices.emplace_back(Vertex{px, py + sy, pz, -1, 0, 0, material});
}

void Face::Right(std::vector<Vertex>& vertices, float px, float py, float pz, float sx, float sy, float sz, uint32_t material) {
  vertices.emplace_back(Vertex{px + sx, py, pz, 1, 0, 0, material});
  vertices.emplace_back(
      Vertex{px + sx, py + sy, pz + sz, 1, 0, 0, material});
  vertices.emplace_back(Vertex{px + sx, py, pz + sz, 1, 0, 0, material});

  vertices.emplace_back(Vertex{px + sx, py, pz, 1, 0, 0, material});
  vertices.emplace_back(Vertex{px + sx, py + sy, pz, 1, 0, 0, material});
  vertices.emplace_back(
      Vertex{px + sx, py + sy, pz + sz, 1, 0, 0, material});
}