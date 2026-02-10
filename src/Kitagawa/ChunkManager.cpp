#include "ChunkManager.h"

#include <execution>

#include "Kitagawa/World.h"
#include "Signal.h"
#include "ThreadPool.h"
#include "Utility/Debug.h"
#include "Voxel/GreedyMesh64.h"

ChunkManager::ChunkManager(uint32_t chunkSize) {
  m_SVO = new SparseOctree<Voxel>();

  for (size_t i = 0; i < SIZE; i++)
    m_Chunks[i] = new SparseOctree<Voxel>();
}

ChunkManager::~ChunkManager() {
  for (size_t i = 0; i < SIZE; i++)
    delete m_Chunks[i];

  delete m_SVO;
}

void ChunkManager::Set(int x, int y, int z, Voxel* data) {
  m_SVO->Set(x, y, z, data);
}

void ChunkManager::Set(const glm::vec3& position, Voxel* data) {
  m_SVO->Set(position, data);
}

void ChunkManager::Clear(const glm::ivec3& position) {
  m_SVO->Clear(position);
}

void ChunkManager::Update(const glm::vec3& origin, const glm::vec3& direction) {
}

void ChunkManager::Sync() {
  m_SVO->Sync();
}

uint64_t ChunkManager::ReadLock() {
  return m_SVO->ReadLock();
}

void ChunkManager::ReadUnlock(uint64_t generation) {
  m_SVO->ReadUnlock(generation);
}

void ChunkManager::Flatten(std::vector<SparseOctree<Voxel>::FlatNode>& out) {
  m_SVO->Flatten(out);
}

void ChunkManager::GreedyMesh(const std::vector<Material>& materials, std::vector<Vertex>& out) {
  m_VertexThreads.resize(materials.size());

  SparseOctree<Voxel>::Node* root = m_SVO->GetRoot(std::memory_order::acquire);

  auto generateVerticies = [&results = m_VertexThreads, root, svo = m_SVO](Material material) {
    uint64_t generation = svo->ReadLock();

    std::vector<Vertex> buffer;

    static thread_local uint8_t padding[64 * 64] = {};
    std::memset(padding, 0, sizeof(padding));

    auto& rows    = svo->GetAxisX(root, material.Id);
    auto& columns = svo->GetAxisY(root, material.Id);
    auto& layers  = svo->GetAxisZ(root, material.Id);

    GreedyMesh64::GeneratePadding(padding, rows, columns, layers, [svo](int x, int y, int z) -> bool {
      if (x >= SparseOctree<Voxel>::SIZE || x < 0)
        return true;
      if (y >= SparseOctree<Voxel>::SIZE || y < 0)
        return true;
      if (z >= SparseOctree<Voxel>::SIZE || z < 0)
        return true;
      return svo->Exists(x, y, z);
    });

    GreedyMesh64::Generate(buffer, {0, 0, 0}, material.Id, rows, columns, layers, padding);

    std::vector<Vertex> tmp;
    tmp.reserve(40000);

    BENCHMARK([&]() {
      GreedyMesh64::Generate(tmp, {0, 0, 0}, material.Id, rows, columns, layers, padding);
    }, 1000);

    results[material.Id - 1] = std::move(buffer);

    svo->ReadUnlock(generation);
  };

  auto onComplete = [&results = m_VertexThreads, &out = out, svo = m_SVO]() {
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

SparseOctree<Voxel>::Hit ChunkManager::DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction) {
  return m_SVO->DeepRaymarch(origin, direction);
}

inline glm::ivec3 ChunkManager::GetCoord(int x, int y, int z) {
  return glm::ivec3{int(x / SparseOctree<Voxel>::SIZE), int(y / SparseOctree<Voxel>::SIZE), int(z / SparseOctree<Voxel>::SIZE)};
}
