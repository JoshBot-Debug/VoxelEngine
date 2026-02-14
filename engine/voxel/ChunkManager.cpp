// #include "ChunkManager.h"

// #include <execution>

// #include "Debug.h"
// #include "World.h"
// #include "thread/Signal.h"
// #include "thread/ThreadPool.h"
// #include "voxel/GreedyMesh64.h"

// ChunkManager::ChunkManager(uint32_t chunkSize) {
//   for (auto& chunk : m_Chunks)
//     chunk = new SparseOctree<Voxel>();

//   for (auto& id : m_GreedyMeshingTask)
//     id = akari::thread::ThreadPool::GenerateId();
// }

// ChunkManager::~ChunkManager() {
//   for (size_t i = 0; i < (SIZE * SIZE * SIZE); i++)
//     delete m_Chunks[i];
// }

// void ChunkManager::Set(const glm::ivec3& lcc, int x, int y, int z, Voxel* data) {
//   uint32_t i = index(lcc, SIZE);
//   m_Chunks[i]->Set(x, y, z, data);
// }

// void ChunkManager::Set(const glm::ivec3& lcc, const glm::ivec3& position, Voxel* data) {
//   uint32_t i = index(lcc, SIZE);
//   m_Chunks[i]->Set(position.x, position.y, position.z, data);
// }

// void ChunkManager::Set(const glm::ivec3& lcc, SparseOctree<Voxel>::Writer& session, int x, int y, int z, Voxel* data) {
//   uint32_t i = index(lcc, SIZE);
//   m_Chunks[i]->Set(session, x, y, z, data);
// }

// void ChunkManager::Clear(const glm::ivec3& lcc, const glm::ivec3& position) {
//   uint32_t i = index(lcc, SIZE);
//   m_Chunks[i]->Clear(position.x, position.y, position.z);
// }

// void ChunkManager::Sync(const glm::ivec3& lcc) {
//   uint32_t i = index(lcc, SIZE);
//   m_Chunks[i]->Sync();
// }

// SparseOctree<Voxel>::Writer ChunkManager::BeginWrite(const glm::ivec3& lcc) {
//   uint32_t i = index(lcc, SIZE);
//   return m_Chunks[i]->BeginWrite();
// }

// SparseOctree<Voxel>::Reader ChunkManager::BeginRead(const glm::ivec3& lcc) {
//   uint32_t i = index(lcc, SIZE);
//   return m_Chunks[i]->BeginRead();
// }

// void ChunkManager::Flatten(SparseOctree<Voxel>::Reader& session, const glm::ivec3& lcc, std::vector<SparseOctree<Voxel>::FlatNode>& out) {
//   uint32_t i = index(lcc, SIZE);
//   m_Chunks[i]->Flatten(session, out);
// }

// void ChunkManager::GreedyMesh(const glm::ivec3& wcc, const glm::ivec3& lcc, const std::vector<Material>& materials, std::vector<Vertex>& out) {
//   uint32_t i                  = index(lcc, SIZE);
//   auto&    chunkVertexThreads = m_VertexThreads[i];
//   uint64_t threadId           = m_GreedyMeshingTask[i];

//   chunkVertexThreads.resize(materials.size());

//   auto generateVerticies = [&chunkVertexThreads, &chunks = m_Chunks, wcc, lcc, i](Material material) {
//     auto& svo     = chunks[i];
//     auto  session = svo->BeginRead();

//     std::vector<Vertex> buffer;
//     uint8_t             padding[64 * 64] = {};

//     auto& rows    = svo->GetAxisX(session, material.Id);
//     auto& columns = svo->GetAxisY(session, material.Id);
//     auto& layers  = svo->GetAxisZ(session, material.Id);

//     GreedyMesh64::GeneratePadding(padding, rows, columns, layers, [&chunks, wcc, lcc, i](int x, int y, int z) -> bool {
//       uint32_t size = SparseOctree<Voxel>::SIZE;
//       uint32_t r    = radius(SIZE);

//       if (x < 0 || y < 0 || z < 0 || x >= size || y >= size || z >= size) {
//         const glm::ivec3 np = {floorDivision(x, size) + lcc.x, floorDivision(y, size) + lcc.y, floorDivision(z, size) + lcc.z};

//         if (std::abs(np.x) > r || std::abs(np.y) > r || std::abs(np.z) > r)
//           return true;

//         uint32_t ni   = index(np, SIZE);
//         auto&    nsvo = chunks[ni];

//         return nsvo->Exists(wrap(x, size), wrap(y, size), wrap(z, size));
//       }

//       return chunks[i]->Exists(x, y, z);
//     });

//     GreedyMesh64::Generate(buffer, wcc + lcc, material.Id, rows, columns, layers, padding);

//     chunkVertexThreads[material.Id - 1] = std::move(buffer);
//   };

//   auto onComplete = [&chunkVertexThreads, &out = out]() {
//     size_t total = 0;

//     for (const auto& v : chunkVertexThreads)
//       total += v.size();

//     // out.clear();
//     out.reserve(out.size() + total);

//     for (auto& v : chunkVertexThreads)
//       out.insert(out.end(), v.begin(), v.end());

//     for (auto& v : chunkVertexThreads)
//       v.clear();

//     akari::thread::Signal::Set(vxen::World::CHUNK_MANAGER_FLUSH_RENDER);
//   };

//   akari::thread::ThreadPool::ForEach(threadId, std::move(materials), generateVerticies, onComplete);
// }

// SparseOctree<Voxel>::Hit ChunkManager::DeepRaymarch(const glm::ivec3& lcc, const glm::vec3& origin, const glm::vec3& direction) {
//   uint32_t i = index(lcc, SIZE);
//   return m_Chunks[i]->DeepRaymarch(origin, direction);
// }

#include "ChunkManager.h"