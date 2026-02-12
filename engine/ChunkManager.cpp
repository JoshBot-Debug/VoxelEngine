#include "ChunkManager.h"

#include <execution>

#include "Debug.h"
#include "World.h"
#include "thread/Signal.h"
#include "thread/ThreadPool.h"
#include "voxel/GreedyMesh64.h"

inline uint32_t radius(uint32_t size) {
  return (size - 1) / 2;
}

inline uint32_t index(const glm::ivec3& coord, uint32_t size) {
  auto p = coord + glm::ivec3(radius(size));
  return p.x + (size * (p.y + (size * p.z)));
}

inline uint32_t index(uint8_t x, uint8_t y, uint8_t z, uint32_t size) {
  uint32_t r = radius(size);
  x += r;
  y += r;
  z += r;
  return x + (size * (y + (size * z)));
}

ChunkManager::ChunkManager(uint32_t chunkSize) {
  m_SVO = new SparseOctree<Voxel>();

  for (auto& chunk : m_Chunks)
    chunk = new SparseOctree<Voxel>();

  for (auto& id : m_GreedyMeshingTask)
    id = akari::thread::ThreadPool::GenerateId();
}

ChunkManager::~ChunkManager() {
  for (size_t i = 0; i < (SIZE * SIZE * SIZE); i++)
    delete m_Chunks[i];

  delete m_SVO;
}

void ChunkManager::Set(const glm::ivec3& coord, int x, int y, int z, Voxel* data) {
  uint32_t i = index(coord, SIZE);
  m_Chunks[i]->Set(x, y, z, data);
}

void ChunkManager::Set(const glm::ivec3& coord, const glm::ivec3& position, Voxel* data) {
  uint32_t i = index(coord, SIZE);
  m_Chunks[i]->Set(position.x, position.y, position.z, data);
}

void ChunkManager::Set(const glm::ivec3& coord, SparseOctree<Voxel>::Writer& session, int x, int y, int z, Voxel* data) {
  uint32_t i = index(coord, SIZE);
  m_Chunks[i]->Set(session, x, y, z, data);
}

void ChunkManager::Clear(const glm::ivec3& coord, const glm::ivec3& position) {
  uint32_t i = index(coord, SIZE);
  m_Chunks[i]->Clear(position.x, position.y, position.z);
}

void ChunkManager::Sync(const glm::ivec3& coord) {
  uint32_t i = index(coord, SIZE);
  m_Chunks[i]->Sync();
}

SparseOctree<Voxel>::Writer ChunkManager::BeginWrite(const glm::ivec3& coord) {
  uint32_t i = index(coord, SIZE);
  return m_Chunks[i]->BeginWrite();
}

SparseOctree<Voxel>::Reader ChunkManager::BeginRead(const glm::ivec3& coord) {
  uint32_t i = index(coord, SIZE);
  return m_Chunks[i]->BeginRead();
}

void ChunkManager::Flatten(SparseOctree<Voxel>::Reader& session, const glm::ivec3& coord, std::vector<SparseOctree<Voxel>::FlatNode>& out) {
  uint32_t i = index(coord, SIZE);
  m_Chunks[i]->Flatten(session, out);
}

void ChunkManager::GreedyMesh(const glm::ivec3& coord, const glm::ivec3& offset, const std::vector<Material>& materials, std::vector<Vertex>& out) {
  uint32_t i = index(coord, SIZE);
  m_VertexThreads[i].resize(materials.size());

  auto generateVerticies = [&results = m_VertexThreads[i], &chunks = m_Chunks, offset, i, coord](Material material) {
    auto& svo     = chunks[i];
    auto  session = svo->BeginRead();

    std::vector<Vertex> buffer;

    uint8_t padding[64 * 64] = {};

    auto& rows    = svo->GetAxisX(session, material.Id);
    auto& columns = svo->GetAxisY(session, material.Id);
    auto& layers  = svo->GetAxisZ(session, material.Id);

    GreedyMesh64::GeneratePadding(padding, rows, columns, layers, [&chunks, i, coord](int x, int y, int z) -> bool {
      uint32_t size      = SparseOctree<Voxel>::SIZE;
      uint32_t chunkSize = ChunkManager::SIZE;
      uint32_t r         = radius(chunkSize);

      if (x == size || x == -1) {
        uint32_t ax = std::abs(coord.x);

        if (ax >= r)
          return true;

        if (x == 64) {
          uint32_t i = index(coord.x + 1, coord.y, coord.z, chunkSize);
          return chunks[i]->Exists(0, y, z);
        }

        if (x == -1) {
          uint32_t i = index(coord.x - 1, coord.y, coord.z, chunkSize);
          return chunks[i]->Exists(63, y, z);
        }

        return true;
      }
      if (y == size || y == -1) {
        uint32_t ay = std::abs(coord.y);

        if (ay >= r)
          return true;

        if (y == 64) {
          uint32_t i = index(coord.x, coord.y + 1, coord.z, chunkSize);
          return chunks[i]->Exists(x, 0, z);
        }

        if (y == -1) {
          uint32_t i = index(coord.x, coord.y - 1, coord.z, chunkSize);
          return chunks[i]->Exists(x, 63, z);
        }
        return true;
      }

      if (z == size || z == -1) {
        uint32_t az = std::abs(coord.z);

        if (az >= r)
          return true;

        if (z == 64) {
          uint32_t i = index(coord.x, coord.y, coord.z + 1, chunkSize);
          return chunks[i]->Exists(x, y, 0);
        }

        if (z == -1) {
          uint32_t i = index(coord.x, coord.y, coord.z - 1, chunkSize);
          return chunks[i]->Exists(x, y, 63);
        }

        return true;
      }

      return chunks[i]->Exists(x, y, z);
    });

    GreedyMesh64::Generate(buffer, offset, material.Id, rows, columns, layers, padding);

    results[material.Id - 1] = std::move(buffer);
  };

  auto onComplete = [&results = m_VertexThreads[i], &out = out]() {
    size_t total = 0;

    for (const auto& v : results)
      total += v.size();

    // out.clear();
    out.reserve(out.size() + total);

    for (auto& v : results)
      out.insert(out.end(), v.begin(), v.end());

    for (auto& v : results)
      v.clear();

    akari::thread::Signal::Set(vxen::World::CHUNK_MANAGER_FLUSH_RENDER);
  };

  akari::thread::ThreadPool::ForEach(m_GreedyMeshingTask[i], std::move(materials), generateVerticies, onComplete);
}

SparseOctree<Voxel>::Hit ChunkManager::DeepRaymarch(const glm::ivec3& coord, const glm::vec3& origin, const glm::vec3& direction) {
  uint32_t i = index(coord, SIZE);
  return m_Chunks[i]->DeepRaymarch(origin, direction);
}