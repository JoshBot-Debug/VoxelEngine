#include "ChunkManager.h"

#include <execution>

#include "Voxel/GreedyMesh64.h"

ChunkManager::ChunkManager(uint32_t chunkSize) {
  m_SVO = new SparseOctree<Voxel>(chunkSize);

  for (size_t i = 0; i < SIZE; i++)
    m_Chunks[i] = new SparseOctree<Voxel>(chunkSize);
}

ChunkManager::~ChunkManager() {
  for (size_t i = 0; i < SIZE; i++)
    delete m_Chunks[i];
}

void ChunkManager::Set(uint64_t (&mask)[], Voxel* data) {
  m_SVO->Set(mask, data);
}

void ChunkManager::Set(int x, int y, int z, Voxel* data, int leafSize) {
  m_SVO->Set(x, y, z, data, leafSize);
}

void ChunkManager::Set(const glm::vec3& position, Voxel* data, int leafSize) {
  m_SVO->Set(position, data, leafSize);
}

void ChunkManager::Clear(const glm::ivec3& position, int leafSize) {
  m_SVO->Clear(position, leafSize);
}

void ChunkManager::Update(const glm::vec3& origin, const glm::vec3& direction) {
}

void ChunkManager::Flush() {
  m_SVO->Flush();
}

bool ChunkManager::IsDirty() {
  return m_SVO->IsDirty();
}

void ChunkManager::Clean() {
  m_SVO->Clean();
}

void ChunkManager::Flatten(std::vector<SparseOctree<Voxel>::FlatNode>& out) {
  m_SVO->Flatten(out);
}

void ChunkManager::GreedyMesh(std::vector<Material> materials, std::vector<Vertex>& out) {
  std::vector<std::vector<Vertex>> results(materials.size());

  const int chunksPerAxis = std::max(1, static_cast<int>(m_SVO->GetSize() / GreedyMesh64::CHUNK_SIZE));

  auto exists = [this](float x, float y, float z, uint32_t id = 0) -> bool {
    return m_SVO->Get(x, y, z, id);
  };

  std::for_each(std::execution::par_unseq, materials.begin(), materials.end(), [&materials, &results, chunksPerAxis, &exists](Material& material) {
    std::vector<Vertex> buffer;

    for (int cz = 0; cz < chunksPerAxis; ++cz)
      for (int cy = 0; cy < chunksPerAxis; ++cy)
        for (int cx = 0; cx < chunksPerAxis; ++cx)
          GreedyMesh64::Generate(exists, buffer, {cx, cy, cz}, material.Id);

    results[material.Id - 1] = std::move(buffer);
  });

  static_assert(std::is_trivially_copyable_v<Vertex>);

  size_t total = 0;
  for (const auto& v : results)
    total += v.size();

  out.clear();
  out.reserve(total);

  for (auto& v : results)
    out.insert(out.end(), v.begin(), v.end());

  results.clear();
}

SparseOctree<Voxel>::Hit ChunkManager::DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction) {
  return m_SVO->DeepRaymarch(origin, direction);
}
