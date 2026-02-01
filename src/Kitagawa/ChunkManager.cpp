#include "ChunkManager.h"

ChunkManager::ChunkManager(uint32_t chunkSize) {
  for (size_t i = 0; i < SIZE; i++)
    m_Chunks[i] = new SparseOctree<Voxel>(chunkSize);
}

ChunkManager::~ChunkManager() {
  for (size_t i = 0; i < SIZE; i++)
    delete m_Chunks[i];
}

void ChunkManager::Set(uint64_t (&mask)[], Voxel* data) {
}

void ChunkManager::Set(int x, int y, int z, Voxel* data, int leafSize) {
}

void ChunkManager::Set(const glm::vec3 &position, Voxel* data, int leafSize) {
}

void ChunkManager::Clear(const glm::ivec3 &position, int leafSize) {
}

void ChunkManager::Update(const glm::vec3& origin, const glm::vec3& direction) {
}

void ChunkManager::Flush() {
}

bool ChunkManager::IsDirty() {
  return false;
}

void ChunkManager::Clean() {
}

void ChunkManager::Flatten(std::vector<SparseOctree<Voxel>::FlatNode>& out) {
}

void ChunkManager::GreedyMesh(const std::vector<Material>& materials, std::vector<Vertex> out) {
}

SparseOctree<Voxel>::Hit ChunkManager::DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction) {
  return SparseOctree<Voxel>::Hit();
}
