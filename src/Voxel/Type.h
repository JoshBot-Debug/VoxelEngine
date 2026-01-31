#pragma once
#include <glm/glm.hpp>

struct alignas(16) DDGIProbe {
  glm::vec4 position;
};

struct alignas(16) FlatVoxel {
  uint32_t Depth      = 0;
  uint32_t Children   = 0;
  uint32_t ChildIndex = 0;
  uint32_t id         = 0;
};

struct alignas(16) DenseVoxel {
  glm::vec3 Position;
  uint32_t  PadP;
  uint32_t  Depth      = 0;
  uint32_t  id         = 0;
  uint32_t  Padding[3] = {};
};

struct alignas(16) Material {
  uint32_t  Id        = 0;
  float     Metallic  = 0.0f;
  float     Roughness = 1.0f;
  uint32_t  Padding   = 0;
  glm::vec4 Albedo    = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  glm::vec4 Emissive  = glm::vec4(0.0f);
};

struct alignas(16) Vertex {
  glm::vec3 Position;
  uint32_t  PadP;
  glm::vec3 Normal;
  uint32_t  PadN;
  uint32_t  Id;
  uint32_t  Padding[3] = {};

  Vertex() = default;

  Vertex(float x, float y, float z, float nx, float ny, float nz) {
    Position.x = x;
    Position.y = y;
    Position.z = z;
    Normal.x   = nx;
    Normal.y   = ny;
    Normal.z   = nz;
  }

  Vertex(float x, float y, float z, float nx, float ny, float nz, uint32_t id)
      : Id(id) {
    Position.x = x;
    Position.y = y;
    Position.z = z;
    Normal.x   = nx;
    Normal.y   = ny;
    Normal.z   = nz;
  }

  bool operator==(const Vertex& other) const {
    return Position.x == other.Position.x && Position.y == other.Position.y &&
           Position.z == other.Position.z && Normal.x == other.Normal.x &&
           Normal.y == other.Normal.y && Normal.z == other.Normal.z;
  }
};

struct AABB {
  glm::vec3 Min;
  glm::vec3 Max;
};

struct Plane {
  glm::vec3 Normal;
  float     Distance;

  Plane() = default;
  Plane(const glm::vec3& n, float distance)
      : Normal(glm::normalize(n))
      , Distance(distance) {}

  float distance(const glm::vec3& point) const {
    return glm::dot(Normal, point) + Distance;
  }
};

struct Frustum {
  enum class Intersection { Outside,
                            Intersecting,
                            Inside };

  Plane Planes[6]; // left, right, bottom, top, near, far

  enum { LEFT = 0,
         RIGHT,
         BOTTOM,
         TOP,
         NEAR,
         FAR };

  static Frustum FromMatrix(const glm::mat4& viewProjection) {
    Frustum f;

    auto makePlane = [](glm::vec4 v) {
      glm::vec3 n(v.x, v.y, v.z);
      float     len = glm::length(n);
      return Plane(n / len, v.w / len);
    };

    glm::vec4 rowX(viewProjection[0][0], viewProjection[1][0], viewProjection[2][0], viewProjection[3][0]);
    glm::vec4 rowY(viewProjection[0][1], viewProjection[1][1], viewProjection[2][1], viewProjection[3][1]);
    glm::vec4 rowZ(viewProjection[0][2], viewProjection[1][2], viewProjection[2][2], viewProjection[3][2]);
    glm::vec4 rowW(viewProjection[0][3], viewProjection[1][3], viewProjection[2][3], viewProjection[3][3]);

    f.Planes[LEFT]   = makePlane(rowW + rowX);
    f.Planes[RIGHT]  = makePlane(rowW - rowX);
    f.Planes[BOTTOM] = makePlane(rowW + rowY);
    f.Planes[TOP]    = makePlane(rowW - rowY);
    f.Planes[NEAR]   = makePlane(rowW + rowZ);
    f.Planes[FAR]    = makePlane(rowW - rowZ);

    return f;
  }

  Intersection Intersects(const AABB& box) const {
    bool intersects = false;

    for (int i = 0; i < 6; i++) {
      const Plane& p = Planes[i];

      glm::vec3 positive = box.Min;
      glm::vec3 negative = box.Max;

      if (p.Normal.x >= 0) {
        positive.x = box.Max.x;
        negative.x = box.Min.x;
      }
      if (p.Normal.y >= 0) {
        positive.y = box.Max.y;
        negative.y = box.Min.y;
      }
      if (p.Normal.z >= 0) {
        positive.z = box.Max.z;
        negative.z = box.Min.z;
      }

      if (p.distance(positive) < 0)
        return Intersection::Outside;

      if (p.distance(negative) < 0)
        intersects = true;
    }

    return intersects ? Intersection::Intersecting : Intersection::Inside;
  }
};