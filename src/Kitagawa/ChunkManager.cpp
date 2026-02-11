#include "ChunkManager.h"

#include <execution>

#include "Kitagawa/World.h"
#include "Signal.h"
#include "ThreadPool.h"
#include "Utility/Debug.h"
#include "Voxel/GreedyMesh64.h"

inline uint32_t index(const glm::ivec3& coord, const glm::ivec3& root, uint32_t size) {
  /// TODO: Need to decide on this
  auto p = (coord + root) - glm::ivec3(1);
  return p.x + (size * (p.y + (size * p.z)));
}

ChunkManager::ChunkManager(uint32_t chunkSize) {
  m_SVO = new SparseOctree<Voxel>();

  for (size_t i = 0; i < (SIZE * SIZE * SIZE); i++)
    m_Chunks[i] = new SparseOctree<Voxel>();
}

ChunkManager::~ChunkManager() {
  for (size_t i = 0; i < (SIZE * SIZE * SIZE); i++)
    delete m_Chunks[i];

  delete m_SVO;
}

void ChunkManager::Set(const glm::ivec3& coord, int x, int y, int z, Voxel* data) {
  uint32_t i       = index(coord, ROOT_POSITION, SIZE);
  uint32_t offsetX = coord.x * SparseOctree<Voxel>::SIZE;
  uint32_t offsetY = coord.y * SparseOctree<Voxel>::SIZE;
  uint32_t offsetZ = coord.z * SparseOctree<Voxel>::SIZE;
  m_Chunks[i]->Set(x + offsetX, y + offsetY, z + offsetZ, data);
}

void ChunkManager::Set(const glm::ivec3& coord, const glm::vec3& position, Voxel* data) {
  uint32_t i       = index(coord, ROOT_POSITION, SIZE);
  uint32_t offsetX = coord.x * SparseOctree<Voxel>::SIZE;
  uint32_t offsetY = coord.y * SparseOctree<Voxel>::SIZE;
  uint32_t offsetZ = coord.z * SparseOctree<Voxel>::SIZE;
  m_Chunks[i]->Set(position.x + offsetX, position.y + offsetY, position.z + offsetZ, data);
}

void ChunkManager::Set(const glm::ivec3& coord, SparseOctree<Voxel>::Writer& session, int x, int y, int z, Voxel* data) {
  uint32_t i       = index(coord, ROOT_POSITION, SIZE);
  uint32_t offsetX = coord.x * SparseOctree<Voxel>::SIZE;
  uint32_t offsetY = coord.y * SparseOctree<Voxel>::SIZE;
  uint32_t offsetZ = coord.z * SparseOctree<Voxel>::SIZE;
  m_Chunks[i]->Set(session, x + offsetX, y + offsetY, z + offsetZ, data);
}

void ChunkManager::Clear(const glm::ivec3& coord, const glm::ivec3& position) {
  uint32_t i       = index(coord, ROOT_POSITION, SIZE);
  uint32_t offsetX = coord.x * SparseOctree<Voxel>::SIZE;
  uint32_t offsetY = coord.y * SparseOctree<Voxel>::SIZE;
  uint32_t offsetZ = coord.z * SparseOctree<Voxel>::SIZE;
  m_Chunks[i]->Clear(position.x + offsetX, position.y + offsetY, position.z + offsetZ);
}

void ChunkManager::Sync(const glm::ivec3& coord) {
  uint32_t i = index(coord, ROOT_POSITION, SIZE);
  m_Chunks[i]->Sync();
}

SparseOctree<Voxel>::Writer ChunkManager::BeginWrite(const glm::ivec3& coord) {
  uint32_t i = index(coord, ROOT_POSITION, SIZE);
  return m_Chunks[i]->BeginWrite();
}

SparseOctree<Voxel>::Reader ChunkManager::BeginRead(const glm::ivec3& coord) {
  uint32_t i = index(coord, ROOT_POSITION, SIZE);
  return m_Chunks[i]->BeginRead();
}

void ChunkManager::Flatten(SparseOctree<Voxel>::Reader& session, const glm::ivec3& coord, std::vector<SparseOctree<Voxel>::FlatNode>& out) {
  uint32_t i = index(coord, ROOT_POSITION, SIZE);
  m_Chunks[i]->Flatten(session, out);
}

void ChunkManager::GreedyMesh(const glm::ivec3& coord, const std::vector<Material>& materials, std::vector<Vertex>& out) {
  uint32_t i = index(coord, ROOT_POSITION, SIZE);
  m_VertexThreads.resize(materials.size());

  auto generateVerticies = [&results = m_VertexThreads, svo = m_Chunks[i], coord](Material material) {
    auto session = svo->BeginRead();

    std::vector<Vertex> buffer;

    uint8_t padding[64 * 64] = {};

    auto& rows    = svo->GetAxisX(session, material.Id);
    auto& columns = svo->GetAxisY(session, material.Id);
    auto& layers  = svo->GetAxisZ(session, material.Id);

    GreedyMesh64::GeneratePadding(padding, rows, columns, layers, [svo](int x, int y, int z) -> bool {
      if (x >= SparseOctree<Voxel>::SIZE || x < 0)
        return true;
      if (y >= SparseOctree<Voxel>::SIZE || y < 0)
        return true;
      if (z >= SparseOctree<Voxel>::SIZE || z < 0)
        return true;
      return svo->Exists(x, y, z);
    });

    GreedyMesh64::Generate(buffer, coord, material.Id, rows, columns, layers, padding);

    results[material.Id - 1] = std::move(buffer);
  };

  auto onComplete = [&results = m_VertexThreads, &out = out]() {
    size_t total = 0;
    for (const auto& v : results)
      total += v.size();

    out.clear();
    out.reserve(total);

    for (auto& v : results)
      out.insert(out.end(), v.begin(), v.end());

    for (auto& v : results)
      v.clear();

    Akari::Signal::Set(Kitagawa::World::CHUNK_MANAGER_FLUSH_RENDER);
  };

  Akari::ThreadPool::ForEach(m_GreedyMeshingTask, std::move(materials), generateVerticies, onComplete);
}

SparseOctree<Voxel>::Hit ChunkManager::DeepRaymarch(const glm::ivec3& coord, const glm::vec3& origin, const glm::vec3& direction) {
  uint32_t i = index(coord, ROOT_POSITION, SIZE);
  return m_SVO->DeepRaymarch(origin, direction);
}