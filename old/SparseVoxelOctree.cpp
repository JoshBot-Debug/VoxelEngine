#include "SparseVoxelOctree.h"

#include <execution>
#include <stack>
#include <tuple>

#include "Voxel/GreedyMesh32.h"
#include "Voxel/GreedyMesh64.h"

SparseVoxelOctree::SparseVoxelOctree()
    : m_Size(256)
    , m_Depth(8)
    , m_Root(new Node(8)) {}

SparseVoxelOctree::SparseVoxelOctree(uint32_t size)
    : m_Size(size)
    , m_Depth(static_cast<uint8_t>(std::log2(size)))
    , m_Root(new Node(static_cast<uint8_t>(std::log2(size)))) {}

SparseVoxelOctree::~SparseVoxelOctree() {
  Clear();
  Node* root = m_Root.load(std::memory_order::acquire);
  delete root;
  root = nullptr;
}

uint32_t SparseVoxelOctree::GetSize() { return m_Size; }

uint32_t SparseVoxelOctree::GetDepth() { return m_Depth; }

Node* SparseVoxelOctree::GetRoot() {
  return m_Root.load(std::memory_order::relaxed);
}

void SparseVoxelOctree::Set(uint64_t (&mask)[], Voxel* voxel) {
  for (int z = 0; z < m_Size; z += 64)
    for (int x = 0; x < m_Size; x += 64)
      for (int y = 0; y < m_Size; y += 64)
        Set(mask, x, y, z, voxel, 64);
}

void SparseVoxelOctree::Set(uint64_t (&mask)[], int x, int y, int z, Voxel* voxel, int size) {
  bool isFullBlock = true;

  for (int dz = 0; dz < size && isFullBlock; ++dz)
    for (int dx = 0; dx < size && isFullBlock; ++dx)
      for (int dy = 0; dy < size && isFullBlock; ++dy) {
        int ix = x + dx;
        int iy = y + dy;
        int iz = z + dz;

        int index = ix + m_Size * (iz + m_Size * iy);
        if (!(mask[index / 64] & (1ULL << (index % 64)))) {
          isFullBlock = false;
          break;
        }
      }

  if (isFullBlock) {
    Set(x, y, z, voxel, size);
    return;
  }

  if (size == 1) {
    int index = x + m_Size * (z + m_Size * y);
    if (mask[index / 64] & (1ULL << (index % 64)))
      Set(x, y, z, voxel, 1);
    return;
  }

  int half = size / 2;

  for (int dz = 0; dz < size; dz += half)
    for (int dx = 0; dx < size; dx += half)
      for (int dy = 0; dy < size; dy += half)
        Set(mask, x + dx, y + dy, z + dz, voxel, half);
}

Node* SparseVoxelOctree::Set(glm::vec3 position, Voxel* voxel, int leafSize) {
  return Set(static_cast<int>(position.x), static_cast<int>(position.y), static_cast<int>(position.z), voxel, leafSize);
}

Node* SparseVoxelOctree::Set(int x, int y, int z, Voxel* voxel, int leafSize) {
  Node* root = m_Root.load(std::memory_order::acquire);
  Node* next = Set(root, x, y, z, voxel, leafSize, m_Size);
  m_Root.store(next, std::memory_order::release);
  return next;
}

Node* SparseVoxelOctree::Set(Node* node, int x, int y, int z, Voxel* voxel, int leafSize, int size) {
  node = new Node(*node);

  if (size == leafSize) {
    node->Voxel = voxel;

    /**
     * Set the voxel and if there are any children, they need to be cleared out
     */
    for (int i = 0; i < 8; i++) {
      /// TODO: Must defer the deletion of nodes
      // delete node->Children[i];
      node->Children[i] = nullptr;
    }

    m_Dirty = true;
    return node;
  }

  int half = size / 2;

  int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

  if (!node->Children[index])
    node->Children[index] = new Node(static_cast<uint8_t>(std::log2(half)));

  node->Children[index] = Set(node->Children[index], x % half, y % half, z % half, voxel, leafSize, half);

  if (node->Voxel)
    node->Voxel = nullptr;

  if (!node->Children[0] || !node->Children[0]->Voxel)
    return node;

  /**
   * If all 8 children exist and are of the same voxel type.
   * Delete all 8 children and set the parent voxel as their type.
   */
  Voxel* firstVoxel = node->Children[0]->Voxel;

  for (int i = 0; i < 8; i++)
    if (!node->Children[i] || !node->Children[i]->Voxel ||
        node->Children[i]->Voxel != firstVoxel)
      return node;

  for (int i = 0; i < 8; i++) {
    /// TODO: Must defer the deletion of nodes
    // delete node->Children[i];
    node->Children[i] = nullptr;
  }

  node->Voxel = firstVoxel;

  return node;
}

Node* SparseVoxelOctree::Get(glm::vec3 position, uint32_t material) {
  return Get(static_cast<int>(position.x), static_cast<int>(position.y), static_cast<int>(position.z), material);
}

Node* SparseVoxelOctree::Get(int x, int y, int z, uint32_t material) {
  Node* root = m_Root.load(std::memory_order::relaxed);
  return Get(root, x, y, z, m_Size, material);
}

Node* SparseVoxelOctree::Get(Node* node, int x, int y, int z, int size, uint32_t material) {

  /// TODO: This "fix" works because I removed neighbour chunks. In reality
  /// I need to check if the x,y,z is somewhere outside of the world, if it is,
  /// I need to return a "void" node because GreedyMesher needs to know if it
  /// has to create faces at the edge of the world (It doesn't need to create a
  /// face there), so if I return a "void" node GreedyMesher thinks "Oh a node
  /// is there, welp, no need to create a face since it's hidden." If the world
  /// is 64x64x64 I query out node at 0,0,64 or 0,0,-1 I should get a void node.
  /// Not a nullptr.
  if (x >= m_Size || x < 0)
    return &m_Void;
  if (y >= m_Size || y < 0)
    return &m_Void;
  if (z >= m_Size || z < 0)
    return &m_Void;

  if (!node)
    return nullptr;

  if (node->Voxel) {
    if (material && material != node->Voxel->Material)
      return nullptr;
    return node;
  }

  if (size == 1)
    return nullptr;

  int half = size / 2;

  int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

  return Get(node->Children[index], x % half, y % half, z % half, half, material);
}

Node* SparseVoxelOctree::Clear(Node*& node, int x, int y, int z, int leafSize, int size, Voxel* filter) {
  /// TODO: After implementing copy on read, this clear is mostly wrong. Need to
  /// investigate

  if (!node)
    return node;

  node = new Node(*node);

  if (size < leafSize)
    return node;

  if (node->Voxel) {
    if (filter && *filter != *node->Voxel)
      return node;

    if (size != m_Size) {
      /// TODO: Must defer this deletion
      delete node;
      node = nullptr;
    } else
      node->Clear();

    m_Dirty = true;
    return node;
  }

  int half = size / 2;

  int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

  node->Children[index] = Clear(node->Children[index], x % half, y % half, z % half, leafSize, half, filter);

  if (size == m_Size)
    return node;

  for (int i = 0; i < 8; i++)
    if (node->Children[i])
      return node;

  /// TODO: Must defer this deletion
  delete node;
  node = nullptr;

  return node;
}

void SparseVoxelOctree::Clear() {
  Node* root = m_Root.load(std::memory_order::acquire);

  if (!root)
    return;

  root->Clear();
}

void SparseVoxelOctree::Clear(glm::ivec3 position, int leafSize, Voxel* filter) {
  Clear(position.x, position.y, position.z, leafSize, filter);
}

void SparseVoxelOctree::Clear(int x, int y, int z, int leafSize, Voxel* target) {
  Node* root = m_Root.load(std::memory_order::acquire);
  Node* next = Clear(root, x, y, z, leafSize, m_Size, target);
  m_Root.store(next, std::memory_order::release);
}

size_t SparseVoxelOctree::GetMemoryUsage(Node* node) {
  if (!node)
    return 0;

  size_t size = sizeof(Node);

  if (node->Voxel)
    size += sizeof(Voxel);

  for (int i = 0; i < 8; ++i)
    size += GetMemoryUsage(node->Children[i]);

  return size;
}

size_t SparseVoxelOctree::GetTotalMemoryUsage() {
  Node* root = m_Root.load(std::memory_order::relaxed);
  return sizeof(SparseVoxelOctree) + GetMemoryUsage(root);
}

void SparseVoxelOctree::Flatten(Node* node) {

  int childCount = -1;

  for (Node* child : node->Children) {
    if (!child)
      continue;

    childCount++;

    uint32_t index = static_cast<uint32_t>(m_Flat.size());

    m_Flat.emplace_back(
        FlatVoxel{.Depth = child->Depth, .Children = 0, .ChildIndex = 0});

    if (child->Voxel)
      m_Flat[index].Material = child->Voxel->Material;

    for (size_t i = 0; i < 8; i++)
      if (child->Children[i])
        m_Flat[index].Children |= (1 << i);
  }

  uint32_t pIndex = static_cast<uint32_t>(m_Flat.size()) - 1;

  for (int i = 0; i < 8; i++) {
    if (!node->Children[i])
      continue;

    uint32_t nIndex = pIndex - childCount--;

    if (m_Flat[nIndex].Depth)
      m_Flat[nIndex].ChildIndex = m_Flat.size();

    Flatten(node->Children[i]);
  }
}

std::vector<FlatVoxel>& SparseVoxelOctree::Flatten() {
  Node* root = m_Root.load(std::memory_order::relaxed);
  m_Dirty    = false;
  m_Flat.clear();

  if (root) {
    uint32_t index = static_cast<uint32_t>(m_Flat.size());

    m_Flat.emplace_back(
        FlatVoxel{.Depth = root->Depth, .Children = 0, .ChildIndex = 1});

    if (root->Voxel)
      m_Flat[index].Material = root->Voxel->Material;

    for (size_t i = 0; i < 8; i++)
      if (root->Children[i])
        m_Flat[index].Children |= (1 << i);

    Flatten(root);
  }

  return m_Flat;
}

std::vector<FlatVoxel>& SparseVoxelOctree::GetFlatTree() { return m_Flat; }

std::vector<DenseVoxel>
SparseVoxelOctree::Filter(const std::function<bool(Node* node)>& filter) {
  std::vector<DenseVoxel> results = {};

  Node* root = m_Root.load(std::memory_order::relaxed);

  Filter(results, filter, {0, 0, 0}, m_Size, root);

  return results;
}

void SparseVoxelOctree::Filter(std::vector<DenseVoxel>&               vector,
                               const std::function<bool(Node* node)>& filter,
                               glm::vec3                              position,
                               int                                    size,
                               Node*                                  node) {
  if (!node)
    return;

  if (node->Voxel && filter(node)) {
    vector.emplace_back(DenseVoxel{
        .Position = position,
        .Depth    = node->Depth,
        .Material = node->Voxel->Material,
    });
    return;
  }

  int childSize = size / 2;

  for (size_t i = 0; i < 8; i++) {
    if (!node->Children[i])
      continue;

    glm::vec3 offset = {
        static_cast<float>((i >> 2) & 1),
        static_cast<float>((i >> 1) & 1),
        static_cast<float>((i >> 0) & 1),
    };

    glm::vec3 childPosition = position + offset * static_cast<float>(childSize);

    Filter(vector, filter, childPosition, childSize, node->Children[i]);
  }
}

std::vector<uint32_t> SparseVoxelOctree::GenerateMipmaps() {

  Node* root = m_Root.load(std::memory_order::relaxed);

  std::vector<uint32_t> result = {};

  uint32_t depth = std::log2(m_Size);

  /// Get the result to the total size required for all the mipmaps
  uint32_t size = 0;
  for (size_t i = 1; i <= depth; i++)
    size += (1ULL << i) * (1ULL << i) * (1ULL << i);

  result.resize(size);

  uint32_t offset = 0;

  for (size_t i = 0; i < depth; i++) {
    FillDenseMipmap(result, offset, (1ULL << i), {0, 0, 0}, m_Size >> i, root);
    uint32_t offsetSize = 1ULL << (depth - i);
    offset += offsetSize * offsetSize * offsetSize;
  }

  return result;
}

void SparseVoxelOctree::FillDenseMipmap(std::vector<uint32_t>& out,
                                        uint32_t               offset,
                                        uint32_t               mipSize,
                                        glm::vec3              position,
                                        int                    size,
                                        Node*                  node) {
  if (!node)
    return;

  if (mipSize == size || node->Voxel) {
    Voxel voxel = AverageVoxel(node);

    uint32_t gridSize = m_Size >> static_cast<uint32_t>(std::log2(mipSize));

    for (int z = 0; z < size; ++z)
      for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
          glm::ivec3 p    = glm::ivec3(position) + glm::ivec3(x, y, z);
          uint64_t   i    = p.x + gridSize * (p.y + gridSize * p.z);
          out[offset + i] = voxel.Material;
        }
    return;
  }

  int childSize = size / 2;

  for (size_t i = 0; i < 8; i++) {
    if (!node->Children[i])
      continue;

    glm::vec3 childOffset = {
        static_cast<float>((i >> 2) & 1),
        static_cast<float>((i >> 1) & 1),
        static_cast<float>((i >> 0) & 1),
    };

    glm::vec3 childPosition =
        position + childOffset * static_cast<float>(childSize);

    FillDenseMipmap(out, offset, mipSize, childPosition, childSize, node->Children[i]);
  }
}

Voxel SparseVoxelOctree::AverageVoxel(Node* node) {

  Voxel voxel;

  if (!node)
    return voxel;

  if (node->Voxel)
    return *node->Voxel;

  size_t                               max = 0;
  std::unordered_map<uint32_t, size_t> frequency;

  std::function<void(Node*)> recurse = [&recurse, &frequency, &voxel, &max](Node* n) {
    if (!n)
      return;

    if (n->Voxel && n->Voxel->Material) {
      size_t count = ++frequency[n->Voxel->Material];
      if (count > max) {
        max            = count;
        voxel.Material = n->Voxel->Material;
      }
    }

    for (size_t i = 0; i < 8; ++i)
      if (n->Children[i])
        recurse(n->Children[i]);
  };

  recurse(node);

  return voxel;
}

std::vector<DDGIProbe> SparseVoxelOctree::GenerateDDGIProbes(uint32_t size) {
  Node* root = m_Root.load(std::memory_order::relaxed);

  std::vector<DDGIProbe> probes;

  GenerateDDGIProbes(probes, static_cast<uint32_t>(std::log2(size)), {0, 0, 0}, m_Size, root);

  m_DDGIProbeSize = probes.size();

  return probes;
}

void SparseVoxelOctree::GenerateDDGIProbes(std::vector<DDGIProbe>& probes,
                                           uint32_t                targetDepth,
                                           glm::vec3               position,
                                           int                     size,
                                           Node*                   node) {

  if (!node)
    return;

  if (targetDepth == node->Depth) {
    position += size / 2;

    if (Get(position))
      return;

    probes.push_back(
        DDGIProbe{glm::vec4(position.x, position.y, position.z, 0.0f)});
    return;
  }

  uint32_t childSize = size / 2;

  for (size_t i = 0; i < 8; i++) {
    if (!node->Children[i])
      continue;

    glm::vec3 childOffset = {
        static_cast<float>((i >> 2) & 1),
        static_cast<float>((i >> 1) & 1),
        static_cast<float>((i >> 0) & 1),
    };

    glm::vec3 childPosition =
        position + childOffset * static_cast<float>(childSize);

    GenerateDDGIProbes(probes, targetDepth, childPosition, childSize, node->Children[i]);
  }
}

uint32_t SparseVoxelOctree::RoundUpToPowerOfTwo(uint32_t n) {
  if (n == 0)
    return 1;
  --n;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return n + 1;
}

std::tuple<uint32_t, uint32_t>
SparseVoxelOctree::ComputeDDGIAtlasSize(uint32_t count, uint32_t texSize, uint32_t& tilesPerRow, uint32_t& tilesPerCol) {
  tilesPerRow =
      static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
  tilesPerCol =
      static_cast<uint32_t>(std::ceil(static_cast<float>(count) / tilesPerRow));

  uint32_t atlasWidth  = RoundUpToPowerOfTwo(tilesPerRow * texSize);
  uint32_t atlasHeight = RoundUpToPowerOfTwo(tilesPerCol * texSize);

  return {atlasWidth, atlasHeight};
}

std::vector<Vertex>
SparseVoxelOctree::GreedyMesh(std::vector<Material> materials) {
  std::vector<std::vector<Vertex>> results(materials.size());

  std::vector<size_t> ids(materials.size());
  std::iota(ids.begin(), ids.end(), 0);

  std::for_each(
      std::execution::par_unseq,
      ids.begin(),
      ids.end(),
      [this, &results, &materials](size_t i) {
        const Material&     material = materials[i];
        std::vector<Vertex> vertices;

        const int chunksPerAxis =
            std::max(1, static_cast<int>(m_Size / GreedyMesh64::CHUNK_SIZE));

        for (int cz = 0; cz < chunksPerAxis; ++cz)
          for (int cy = 0; cy < chunksPerAxis; ++cy)
            for (int cx = 0; cx < chunksPerAxis; ++cx)
              GreedyMesh64::Generate(this, vertices, cx, cy, cz, material.Id);

        results[i] = std::move(vertices);
      });

  std::vector<Vertex> result = {};

  for (auto& v : results)
    result.insert(result.end(), std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()));

  return result;
}

void SparseVoxelOctree::TraverseNodes(std::function<void(Node* node)> callback,
                                      glm::vec3                       position,
                                      int                             size,
                                      Node*                           node) {

  if (!node)
    return;

  callback(node);

  uint32_t childSize = size / 2;

  for (size_t i = 0; i < 8; i++) {
    if (!node->Children[i])
      continue;

    glm::vec3 childOffset = {
        static_cast<float>((i >> 2) & 1),
        static_cast<float>((i >> 1) & 1),
        static_cast<float>((i >> 0) & 1),
    };

    glm::vec3 childPosition =
        position + childOffset * static_cast<float>(childSize);

    TraverseNodes(callback, childPosition, childSize, node->Children[i]);
  }
}

void SparseVoxelOctree::FrustumCull(const Frustum& frustum, glm::vec3 position, int size, Node* node) {
  if (!node)
    return;

  node->IsCulled = false;

  Frustum::Intersection intersection = frustum.Intersects({
      .Min = position,
      .Max = position + static_cast<float>(size),
  });

  if (intersection == Frustum::Intersection::Inside) {
    auto resetCull = [](Node* node) { node->IsCulled = false; };
    TraverseNodes(resetCull, position, size, node);
    return;
  }

  if (intersection == Frustum::Intersection::Outside) {
    node->IsCulled = true;
    return;
  }

  uint32_t childSize = size / 2;

  for (size_t i = 0; i < 8; i++) {
    if (!node->Children[i])
      continue;

    glm::vec3 childOffset = {
        static_cast<float>((i >> 2) & 1),
        static_cast<float>((i >> 1) & 1),
        static_cast<float>((i >> 0) & 1),
    };

    glm::vec3 childPosition =
        position + childOffset * static_cast<float>(childSize);

    FrustumCull(frustum, childPosition, childSize, node->Children[i]);
  }
}

void SparseVoxelOctree::FrustumCull(const Frustum& frustum) {
  Node* root = m_Root.load(std::memory_order::relaxed);
  FrustumCull(frustum, glm::vec3(0.0f), m_Size, root);
}

bool SparseVoxelOctree::intersectAABB(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& min, const glm::vec3& max, float& tMin, float& tMax, glm::vec3& outNormal) {
  tMin      = 0.0f;
  tMax      = 1e30f;
  outNormal = glm::vec3(0.0f);

  for (int i = 0; i < 3; i++)
    if (direction[i] != 0.0f) {
      float invD = 1.0f / direction[i];
      float t1   = (min[i] - origin[i]) * invD;
      float t2   = (max[i] - origin[i]) * invD;

      glm::vec3 n1(0.0f), n2(0.0f);
      n1[i] = -1.0f;
      n2[i] = 1.0f;

      if (t1 > t2) {
        std::swap(t1, t2);
        std::swap(n1, n2);
      }

      if (t1 > tMin) {
        tMin      = t1;
        outNormal = n1;
      }

      tMax = std::min(tMax, t2);

      if (tMin > tMax)
        return false;
    } else if (origin[i] < min[i] || origin[i] > max[i])
      return false;

  return true;
}

SparseVoxelOctree::Hit SparseVoxelOctree::Raymarch(Node* node, const glm::vec3& origin, const glm::vec3& direction, glm::vec3 nodeMin, uint32_t size) {

  float     tMin, tMax;
  glm::vec3 normal;

  if (!intersectAABB(origin, direction, nodeMin, nodeMin + glm::vec3(size), tMin, tMax, normal))
    return Hit();

  if (node->Voxel)
    return Hit{
        .Position = nodeMin,
        .Normal   = normal,
        .Voxel    = node->Voxel,
        .Size     = size,
    };

  if (!node->Children)
    return Hit();

  float half = size / 2.0f;

  int dirX = direction.x >= 0.0f ? 0 : 1;
  int dirY = direction.y >= 0.0f ? 0 : 1;
  int dirZ = direction.z >= 0.0f ? 0 : 1;

  int order[8];

  order[0] = ((0U ^ dirX) << 2) | ((0U ^ dirY) << 1) | (0U ^ dirZ);
  order[1] = ((0U ^ dirX) << 2) | ((0U ^ dirY) << 1) | (1U ^ dirZ);
  order[2] = ((0U ^ dirX) << 2) | ((1U ^ dirY) << 1) | (0U ^ dirZ);
  order[3] = ((0U ^ dirX) << 2) | ((1U ^ dirY) << 1) | (1U ^ dirZ);
  order[4] = ((1U ^ dirX) << 2) | ((0U ^ dirY) << 1) | (0U ^ dirZ);
  order[5] = ((1U ^ dirX) << 2) | ((0U ^ dirY) << 1) | (1U ^ dirZ);
  order[6] = ((1U ^ dirX) << 2) | ((1U ^ dirY) << 1) | (0U ^ dirZ);
  order[7] = ((1U ^ dirX) << 2) | ((1U ^ dirY) << 1) | (1U ^ dirZ);

  for (int j = 0; j < 8; j++) {
    int i = order[j];

    Node* child = node->Children[i];

    if (!child)
      continue;

    glm::vec3 offset = glm::vec3((i >> 2) & 1, (i >> 1) & 1, (i >> 0) & 1);

    glm::vec3 childMin = nodeMin + offset * half;

    Hit hit = Raymarch(child, origin, direction, childMin, half);

    if (hit.Voxel)
      return hit;
  }

  return Hit();
}

SparseVoxelOctree::Hit SparseVoxelOctree::DeepRaymarch(Node* node, const glm::vec3& origin, const glm::vec3& direction, glm::vec3 nodeMin, uint32_t size, Voxel* voxel) {

  float     tMin, tMax;
  glm::vec3 normal;

  if (!intersectAABB(origin, direction, nodeMin, nodeMin + glm::vec3(size), tMin, tMax, normal))
    return Hit();

  voxel = (node && node->Voxel) ? node->Voxel : voxel;

  if (size == 1)
    return Hit{
        .Position = nodeMin,
        .Normal   = normal,
        .Voxel    = voxel,
        .Size     = size,
    };

  float half = size / 2.0f;

  int dirX = direction.x >= 0.0f ? 0 : 1;
  int dirY = direction.y >= 0.0f ? 0 : 1;
  int dirZ = direction.z >= 0.0f ? 0 : 1;

  int order[8];

  order[0] = ((0U ^ dirX) << 2) | ((0U ^ dirY) << 1) | (0U ^ dirZ);
  order[1] = ((0U ^ dirX) << 2) | ((0U ^ dirY) << 1) | (1U ^ dirZ);
  order[2] = ((0U ^ dirX) << 2) | ((1U ^ dirY) << 1) | (0U ^ dirZ);
  order[3] = ((0U ^ dirX) << 2) | ((1U ^ dirY) << 1) | (1U ^ dirZ);
  order[4] = ((1U ^ dirX) << 2) | ((0U ^ dirY) << 1) | (0U ^ dirZ);
  order[5] = ((1U ^ dirX) << 2) | ((0U ^ dirY) << 1) | (1U ^ dirZ);
  order[6] = ((1U ^ dirX) << 2) | ((1U ^ dirY) << 1) | (0U ^ dirZ);
  order[7] = ((1U ^ dirX) << 2) | ((1U ^ dirY) << 1) | (1U ^ dirZ);

  for (int j = 0; j < 8; j++) {
    int i = order[j];

    Node* child = node ? node->Children[i] : nullptr;

    glm::vec3 offset = glm::vec3((i >> 2) & 1, (i >> 1) & 1, (i >> 0) & 1);

    glm::vec3 childMin = nodeMin + offset * half;

    Hit hit = DeepRaymarch(child, origin, direction, childMin, half, voxel);

    if (hit.Voxel)
      return hit;
  }

  return Hit();
}

SparseVoxelOctree::Hit SparseVoxelOctree::Raymarch(glm::vec3 origin, glm::vec3 direction) {
  return Raymarch(m_Root, origin, direction, glm::vec3(0.), m_Size);
}

SparseVoxelOctree::Hit SparseVoxelOctree::DeepRaymarch(glm::vec3 origin, glm::vec3 direction) {
  return DeepRaymarch(m_Root, origin, direction, glm::vec3(0.), m_Size);
}
