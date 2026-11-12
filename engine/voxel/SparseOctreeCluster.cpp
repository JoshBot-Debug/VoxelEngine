#include "SparseOctreeCluster.h"
#include "Type.h"

#include <algorithm>
#include <map>
#include <unordered_set>

Node* SparseOctreeCluster::Ensure(uint8_t x, uint8_t y, uint8_t z) {
  Node* node = m_SCO->Get(x, y, z);
  if (node)
    return node;
  uint32_t cid = m_Trees.size() + 1;
  m_Trees.emplace_back(std::make_unique<SparseOctree>(glm::u8vec3 {x, y, z}));
  m_Vertices.emplace_back();
  m_SCO->Set(x, y, z, cid);
  return m_SCO->Get(x, y, z);
}

SparseOctreeCluster::SparseOctreeCluster()
    : m_SCO(std::make_unique<SparseOctree>()) {}

inline std::pair<glm::u8vec3, glm::u8vec3> WorldPositionToLocalPosition(uint32_t x, uint32_t y, uint32_t z) {
  constexpr uint32_t SIZE = SparseOctree::SIZE;
  const glm::u8vec3  coordinate {x / SIZE, y / SIZE, z / SIZE};
  const glm::u8vec3  position = {x % SIZE, y % SIZE, z % SIZE};
  return {coordinate, position};
}

void SparseOctreeCluster::Set(uint32_t x, uint32_t y, uint32_t z, uint32_t id) {
  auto [coordinate, position] = WorldPositionToLocalPosition(x, y, z);
  auto node                   = Ensure(coordinate.x, coordinate.y, coordinate.z);
  m_Trees[node->Id - 1]->Set(position.x, position.y, position.z, id);
}

Node* SparseOctreeCluster::Get(uint32_t x, uint32_t y, uint32_t z) {
  auto [coordinate, position] = WorldPositionToLocalPosition(x, y, z);
  auto node                   = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  return m_Trees[node->Id - 1]->Get(position.x, position.y, position.z);
}

bool SparseOctreeCluster::Exists(uint32_t x, uint32_t y, uint32_t z) {
  auto [coordinate, position] = WorldPositionToLocalPosition(x, y, z);

  if (!m_SCO->Exists(coordinate.x, coordinate.y, coordinate.z))
    return false;

  auto node = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  return m_Trees[node->Id - 1]->Exists(position.x, position.y, position.z);
}

void SparseOctreeCluster::Clear(uint32_t x, uint32_t y, uint32_t z) {
  auto [coordinate, position] = WorldPositionToLocalPosition(x, y, z);

  if (!m_SCO->Exists(coordinate.x, coordinate.y, coordinate.z))
    return;

  auto node = m_SCO->Get(coordinate.x, coordinate.y, coordinate.z);
  m_Trees[node->Id - 1]->Clear(position.x, position.y, position.z);
}

inline float TNextBoundary(const glm::vec3& origin, const glm::vec3& direction) {
  constexpr float size = SparseOctree::SIZE;

  glm::vec3 nextBoundary = glm::mix(
      glm::vec3(0.0f),
      glm::vec3(size),
      glm::greaterThan(direction, glm::vec3(0.0f)));

  glm::vec3 tVals = (nextBoundary - origin) / direction;

  tVals = glm::mix(
      glm::vec3(1e30f),
      tVals,
      glm::notEqual(direction, glm::vec3(0.0f)));

  return std::min(tVals.x, std::min(tVals.y, tVals.z));
}

SparseOctree::Hit SparseOctreeCluster::Raymarch(const glm::vec3& origin, const glm::vec3& direction) {
  float           t    = 0.0f;
  float           EPS  = 0.001f;
  constexpr float SIZE = SparseOctree::SIZE;

  auto step = [&](auto&& self, float& t) {
    glm::vec3 P = origin + direction * t;

    SparseOctree::Hit chunk = m_SCO->DeepRaymarch(P / SIZE, direction);

    if (!chunk.Id)
      return SparseOctree::Hit();

    // Chunk hit distance relative to current origin
    // 0.001f is to nudge the ray forward a bit so we don't get stuck outside the chunk
    // chunkT moves us into the first chunk we find, we should no get stuck at 64.0f, EPS moves us to 63.0f
    float chunkT = (chunk.TMin * SIZE) + EPS;

    // Ray position inside chunk
    glm::vec3 chunkOrigin = glm::mod(P + direction * chunkT, SIZE);

    SparseOctree::Hit hit = m_Trees[chunk.Id - 1]->DeepRaymarch(chunkOrigin, direction);

    if (hit.Id) {
      hit.TMin += chunkT + t;
      hit.Position += chunk.Position * SIZE;
      return hit;
    }

    // Distance available inside this chunk
    float chunkDist = TNextBoundary(chunkOrigin, direction);

    // Continue past this chunk
    // EPS is to nudge the ray 1 block passed where ever it hits, so we don't get stuck on a boundary inside the same chunk.
    // It moves us to the next chunk (that's the purpose of TNextBoundary but it returns the exact value)
    t += chunkT + chunkDist + EPS;

    return self(self, t);
  };

  return step(step, t);
}

const std::vector<Vertex>& SparseOctreeCluster::GreedyMesh(uint8_t x, uint8_t y, uint8_t z) {

  static const std::vector<Vertex> empty;

  if (!m_SCO->Exists(x, y, z))
    return empty;

  uint32_t      chunkIndex = m_SCO->Get(x, y, z)->Id - 1;
  SparseOctree* tree       = m_Trees[chunkIndex].get();
  auto&         vertices   = m_Vertices[chunkIndex];

  if (!tree->Dirty())
    return vertices;

  vertices.clear();

  constexpr int32_t SIZE = static_cast<int32_t>(SparseOctree::SIZE);

  using FaceGroup = std::tuple<int, int, int, uint32_t>;
  // axis, plane, direction, material -> face cells (u, v)
  std::map<FaceGroup, std::vector<glm::ivec2>> faces;

  auto Encode = [](int u, int v) -> uint64_t {
    return (uint64_t(uint32_t(u)) << 32) | uint32_t(v);
  };

  const glm::u8vec3& coord = tree->GetCoordinate();

  glm::ivec3 chunkBase {coord.x * SIZE, coord.y * SIZE, coord.z * SIZE};

  for (int x = 0; x < SIZE; ++x) {
    for (int y = 0; y < SIZE; ++y) {
      for (int z = 0; z < SIZE; ++z) {

        if (!tree->Exists(x, y, z))
          continue;

        Node* voxel = tree->Get(x, y, z);

        glm::ivec3 world = chunkBase + glm::ivec3(x, y, z);

        for (int axis = 0; axis < 3; ++axis) {
          int uAxis = (axis + 1) % 3;
          int vAxis = (axis + 2) % 3;

          for (int dir = -1; dir <= 1; dir += 2) {
            glm::ivec3 neighbor = world;
            neighbor[axis] += dir;

            // Cull internal and cross-chunk faces
            if (neighbor.x < 0 || neighbor.y < 0 || neighbor.z < 0 || Exists(neighbor.x, neighbor.y, neighbor.z))
              continue;

            int plane = world[axis] + (dir > 0 ? 1 : 0);

            FaceGroup key {axis, plane, dir, voxel->Id};

            faces[key].emplace_back(world[uAxis], world[vAxis]);
          }
        }
      }
    }
  }

  for (auto& [key, cells] : faces) {
    const auto [axis, plane, dir, material] = key;

    const int uAxis = (axis + 1) % 3;
    const int vAxis = (axis + 2) % 3;

    std::unordered_set<uint64_t> remaining;
    remaining.reserve(cells.size());

    for (const auto& cell : cells)
      remaining.insert(Encode(cell.x, cell.y));

    for (const auto& cell : cells) {
      const int u = cell.x;
      const int v = cell.y;

      uint64_t seed = Encode(u, v);

      // Already merged
      if (!remaining.contains(seed))
        continue;

      // --------------------------------------------------------
      // Determine width
      // --------------------------------------------------------
      int width = 1;

      while (remaining.contains(Encode(u + width, v)))
        ++width;

      // Remove first row
      for (int k = 0; k < width; ++k)
        remaining.erase(Encode(u + k, v));

      // --------------------------------------------------------
      // Determine height
      // --------------------------------------------------------
      int height = 1;

      while (true) {
        bool valid = true;

        for (int k = 0; k < width; ++k) {
          if (!remaining.contains(Encode(u + k, v + height))) {
            valid = false;
            break;
          }
        }

        if (!valid)
          break;

        // Remove merged row
        for (int k = 0; k < width; ++k)
          remaining.erase(Encode(u + k, v + height));

        ++height;
      }

      // --------------------------------------------------------
      // Emit quad in world space
      // --------------------------------------------------------
      glm::vec3 normal(0.0f);
      normal[axis] = float(dir);

      glm::vec3 basePos(0.0f);
      basePos[axis]  = float(plane);
      basePos[uAxis] = float(u);
      basePos[vAxis] = float(v);

      glm::vec3 du(0.0f);
      glm::vec3 dv(0.0f);

      du[uAxis] = float(width);
      dv[vAxis] = float(height);

      glm::vec3 v0 = basePos;
      glm::vec3 v1 = basePos + du;
      glm::vec3 v2 = basePos + du + dv;
      glm::vec3 v3 = basePos + dv;

      auto Emit = [&](const glm::vec3& p) {
        vertices.emplace_back(p.x, p.y, p.z, normal.x, normal.y, normal.z, material);
      };

      if (dir > 0) {
        Emit(v0);
        Emit(v1);
        Emit(v2);
        Emit(v0);
        Emit(v2);
        Emit(v3);
      } else {
        Emit(v0);
        Emit(v2);
        Emit(v1);
        Emit(v0);
        Emit(v3);
        Emit(v2);
      }
    }
  }

  return vertices;
}

SparseOctree* SparseOctreeCluster::SVO() {
  return m_SCO.get();
}

const std::vector<std::unique_ptr<SparseOctree>>& SparseOctreeCluster::Clusters() {
  return m_Trees;
}
