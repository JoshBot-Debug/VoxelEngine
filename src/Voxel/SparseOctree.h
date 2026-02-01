#pragma once

#include <atomic>
#include <execution>
#include <glm/glm.hpp>
#include <vector>

#include "Utility/Debug.h"
#include "Voxel/GreedyMesh64.h"

template <typename T>
concept Data = requires(T t) {
  requires std::same_as<decltype(t.Id), uint32_t>;
};

/**
 * @tparam T The datatype of the pointer stored
 */
template <Data T>
class SparseOctree {

public:
  /**
   * The hit struct for raymarching
   * @tparam T The datatype of the pointer stored
   */
  struct Hit {
    glm::vec3 Position = glm::vec3(0.);
    glm::vec3 Normal   = glm::vec3(0.);
    T*        Data     = nullptr;
    uint32_t  Size     = 0;
  };

  /**
   * The nodes in the SVO
   * @tparam T The datatype of the pointer stored
   */
  struct Node {
    uint8_t Depth       = 0;
    T*      Data        = nullptr;
    Node*   Children[8] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    Node() = default;

    Node(uint8_t depth, T* data = nullptr)
        : Depth(depth)
        , Data(data){};

    ~Node() { Clear(); };

    bool operator==(const Node& other) const { return Data.Id == other.Data.Id; };
    bool operator!=(const Node& other) const { return Data.Id != other.Data.Id; };

    void Clear() {
      Data = nullptr;

      for (int i = 0; i < 8; i++)
        if (Children[i]) {
          delete Children[i];
          Children[i] = nullptr;
        }
    };
  };

  struct alignas(16) FlatNode {
    uint32_t Id         = 0;
    uint32_t Depth      = 0;
    uint32_t Children   = 0;
    uint32_t ChildIndex = 0;
  };

  struct alignas(16) FilterNode {
    uint32_t  Id       = 0;
    uint32_t  Depth    = 0;
    glm::vec3 Position = glm::vec3(0.0f);
  };

private:
  /**
   * The total side length of the root node's region.
   * For example, 256 means the root covers a 256×256×256 volume.
   */
  uint32_t m_Size;

  /**
   * The depth of the octree, typically std::log2(m_Size).
   * Indicates how many levels of subdivision exist.
   */
  uint8_t m_Depth;

  /**
   * Pointer to the root node of the Sparse Voxel Octree.
   * Should never be empty, is always initialized in the constructor.
   */
  std::atomic<Node*> m_Root = nullptr;

  /**
   * Tracks wheather the tree has changed.
   */
  bool m_Dirty = false;

  /**
   * VoidNode
   * i.e, the nodes outside of the world, if there is no neighbouring chunk
   */
  Node m_Void = Node();

  std::vector<Node*> m_Release = {};

private:
  /**
   * Internal recursive setter that applies a voxel to all positions marked in
   * the bitmask. Called by the public `set(mask, voxel)` method.
   *
   * @param mask   A bitmask indicating which voxels to set.
   * @param x,y,z  The origin (offset) position in voxel-space for this mask block.
   * @tparam T     The datatype to set at the marked positions.
   * @param size   The current size of the region being processed.
   */
  void Set(uint64_t (&mask)[], int x, int y, int z, T* data, int size) {
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
      Set(x, y, z, data, size);
      return;
    }

    if (size == 1) {
      int index = x + m_Size * (z + m_Size * y);
      if (mask[index / 64] & (1ULL << (index % 64)))
        Set(x, y, z, data, 1);
      return;
    }

    int half = size / 2;

    for (int dz = 0; dz < size; dz += half)
      for (int dx = 0; dx < size; dx += half)
        for (int dy = 0; dy < size; dy += half)
          Set(mask, x + dx, y + dy, z + dz, data, half);
  };

  /**
   * Internal recursive setter that traverses and builds the tree as needed.
   * Called by the public `set(x, y, z, voxel)` and `set(vec3, voxel)` methods.
   *
   * @param node      Current node in the octree.
   * @param x,y,z     Local voxel-space coordinates at this level.
   * @tparam T        The datatype to set at the marked positions.
   * @param leafSize  The target size of a leaf node (typically 1).
   * @param size      The size of the region represented by this node.
   */
  Node* Set(Node* node, int x, int y, int z, T* data, int leafSize, int size) {
    // node = new Node(*node);

    if (size == leafSize) {
      node->Data = data;

      /**
       * Set the voxel and if there are any children, they need to be cleared out
       */
      for (int i = 0; i < 8; i++) {
        /// TODO: Must defer the deletion of nodes
        delete node->Children[i];
        node->Children[i] = nullptr;
      }

      m_Dirty = true;
      return node;
    }

    int half = size / 2;

    int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

    if (!node->Children[index])
      node->Children[index] = new Node(static_cast<uint8_t>(std::log2(half)));

    node->Children[index] = Set(node->Children[index], x % half, y % half, z % half, data, leafSize, half);

    if (node->Data)
      node->Data = nullptr;

    if (!node->Children[0] || !node->Children[0]->Data)
      return node;

    /**
     * If all 8 children exist and are of the same voxel type.
     * Delete all 8 children and set the parent voxel as their type.
     */
    T* first = node->Children[0]->Data;

    for (int i = 0; i < 8; i++)
      if (!node->Children[i] || !node->Children[i]->Data || node->Children[i]->Data != first)
        return node;

    for (int i = 0; i < 8; i++) {
      /// TODO: Must defer the deletion of nodes
      delete node->Children[i];
      node->Children[i] = nullptr;
    }

    node->Data = data;

    return node;
  };

  /**
   * Internal recursive getter that traverses the tree to find a voxel at the
   * given position. Called by the public `get(x, y, z)` and `get(vec3)`
   * methods.
   *
   * @param node    Current node in the octree.
   * @param x,y,z   Local voxel-space coordinates at this level.
   * @param size    The size of the region represented by this node.
   * @param nodeId  Optional filter; only returns nodes matching this id.
   * @return        Pointer to the node at the target position, or nullptr if
   * not found or filtered out.
   */
  Node* Get(Node* node, int x, int y, int z, int size, uint32_t nodeId = 0) {
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

    if (node->Data) {
      if (nodeId && nodeId != node->Data->Id)
        return nullptr;
      return node;
    }

    if (size == 1)
      return nullptr;

    int half = size / 2;

    int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

    return Get(node->Children[index], x % half, y % half, z % half, half, nodeId);
  };

  /**
   * Internal recursive deleter that traverses the octree and removes the voxel
   * at the specified position, if it exists and optionally matches the given
   * target voxel type.
   *
   * Called by the public-facing `clear(x, y, z)` method to safely delete
   * leaf nodes while maintaining octree integrity.
   *
   * @param node      Reference to the current node pointer in the octree. May
   *                  be deleted and set to nullptr if cleared.
   * @param x, y, z   Local voxel-space coordinates at this level.
   * @param leafSize  Minimum size representing a leaf node; used to terminate recursion.
   * @param size      The size of the region represented by this node.
   */
  Node* Clear(Node*& node, int x, int y, int z, int leafSize, int size) {
    if (!node)
      return node;

    if (size < leafSize)
      return node;

    if (node->Data) {
      if (size != m_Size) {
        delete node;
        node = nullptr;
      } else
        node->Clear();

      m_Dirty = true;
      return node;
    }

    int half = size / 2;

    int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

    node->Children[index] = Clear(node->Children[index], x % half, y % half, z % half, leafSize, half);

    if (size == m_Size)
      return node;

    for (int i = 0; i < 8; i++)
      if (node->Children[i])
        return node;

    delete node;
    node = nullptr;

    return node;
  };

  /**
   * Returns the total memory used in bytes by this node and all it's children.
   */
  size_t GetMemoryUsage(Node* node) {
    if (!node)
      return 0;

    size_t size = sizeof(T);

    if (node->Data)
      size += sizeof(T);

    for (int i = 0; i < 8; ++i)
      size += GetMemoryUsage(node->Children[i]);

    return size;
  };

  /**
   * Flatten the tree into a vector that contains all internal nodes
   * leading up to the existing leaf nodes.
   *
   * @param node A pointer to the node to start flattening from
   */
  void Flatten(Node* node, std::vector<FlatNode>& out) {

    int childCount = -1;

    for (Node* child : node->Children) {
      if (!child)
        continue;

      childCount++;

      uint32_t index = static_cast<uint32_t>(out.size());

      out.emplace_back(FlatNode{.Depth = child->Depth, .Children = 0, .ChildIndex = 0});

      if (child->Data)
        out[index].Id = child->Data->Id;

      for (size_t i = 0; i < 8; i++)
        if (child->Children[i])
          out[index].Children |= (1 << i);
    }

    uint32_t pIndex = static_cast<uint32_t>(out.size()) - 1;

    for (int i = 0; i < 8; i++) {
      if (!node->Children[i])
        continue;

      uint32_t nIndex = pIndex - childCount--;

      if (out[nIndex].Depth)
        out[nIndex].ChildIndex = out.size();

      Flatten(node->Children[i], out);
    }
  };

  /**
   * Filter the tree for selected nodes
   *
   * @param vector Out vector
   * @param filter Filter callback
   * @param position Position of the node
   * @param size Size of the node
   * @param node Pointer to the node from which to start a recursive search
   */
  void Filter(const std::function<bool(Node* node)>& filter, std::vector<FilterNode>& out, glm::vec3 position, int size, Node* node) {
    if (!node)
      return;

    if (node->Data && filter(node)) {
      out.emplace_back(FilterNode{
          .Id       = node->Data->Id,
          .Depth    = node->Depth,
          .Position = position,
      });
      return;
    }

    int half = size / 2;

    for (size_t i = 0; i < 8; i++) {
      if (!node->Children[i])
        continue;

      glm::vec3 offset = glm::vec3((i >> 2) & 1, (i >> 1) & 1, (i >> 0) & 1);

      glm::vec3 childMin = position + offset * static_cast<float>(half);

      Filter(filter, out, childMin, half, node->Children[i]);
    }
  };

  /**
   * Perform an AABB intersection test
   */
  bool intersectAABB(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& min, const glm::vec3& max, float& tMin, float& tMax, glm::vec3& outNormal) {
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
  };

  /**
   * Perform a recursive raymarch on the SVO
   */
  Hit Raymarch(Node* node, const glm::vec3& origin, const glm::vec3& direction, glm::vec3 nodeMin, uint32_t size) {

    float     tMin, tMax;
    glm::vec3 normal;

    if (!intersectAABB(origin, direction, nodeMin, nodeMin + glm::vec3(size), tMin, tMax, normal))
      return Hit();

    if (node->Data)
      return Hit{
          .Position = nodeMin,
          .Normal   = normal,
          .Data     = node->Data,
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

      if (hit.Data)
        return hit;
    }

    return Hit();
  };

  /**
   * Perform a recursive raymarch on the SVO
   */
  Hit DeepRaymarch(Node* node, const glm::vec3& origin, const glm::vec3& direction, glm::vec3 nodeMin, uint32_t size, T* data = nullptr) {

    float     tMin, tMax;
    glm::vec3 normal;

    if (!intersectAABB(origin, direction, nodeMin, nodeMin + glm::vec3(size), tMin, tMax, normal))
      return Hit();

    data = (node && node->Data) ? node->Data : data;

    if (size == 1)
      return Hit{
          .Position = nodeMin,
          .Normal   = normal,
          .Data     = data,
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

      Hit hit = DeepRaymarch(child, origin, direction, childMin, half, data);

      if (hit.Data)
        return hit;
    }

    return Hit();
  };

public:
  /**
   * Constructs an empty Sparse Voxel Octree with default settings.
   */
  SparseOctree()
      : m_Size(256)
      , m_Depth(8)
      , m_Root(new Node(8)){};

  /**
   * Constructs a Sparse Voxel Octree with the given spatial size.
   *
   * @param size  The side length of the root node's region (e.g., 256 for a
   * 256³ volume).
   */
  SparseOctree(uint32_t size)
      : m_Size(size)
      , m_Depth(static_cast<uint8_t>(std::log2(size)))
      , m_Root(new Node(static_cast<uint8_t>(std::log2(size)))){};

  /**
   * Destroys the Sparse Voxel Octree and frees all allocated nodes.
   */
  ~SparseOctree() {
    Clear();
    Node* root = m_Root.load(std::memory_order::acquire);
    delete root;
  };

  /**
   * Returns the total size (width, height, depth) of the root node.
   *
   * @return The root node's spatial size (e.g., 256).
   */
  uint32_t GetSize() { return m_Size; };

  /**
   * Efficiently sets multiple voxels in the SVO using a 3D bitmask.
   *
   * This method is optimized for bulk voxel insertion and is significantly
   * faster than calling `set()` for every individual voxel. Instead of
   * iterating over x, y, z coordinates and inserting one voxel at a time, this
   * function uses a bitmask to mark the positions of voxels to be set and
   * inserts them efficiently with minimal tree traversal.
   *
   * The user is responsible for creating and populating the bitmask before
   * calling this method. The bitmask should be in a flattened 3D format
   * (x + size * (z + size * y)), with one bit representing each voxel.
   *
   * Example usage:
   *
   *   const int ChunkSize = 256;
   *   uint64_t mask[(ChunkSize * ChunkSize) * (ChunkSize / 64)] = {0};
   *
   *   for (int z = 0; z < ChunkSize; z++)
   *     for (int x = 0; x < ChunkSize; x++)
   *        for (int y= 0; y < ChunkSize; y++)
   *        {
   *            int index = x + size * (z + (size * y));
   *
   *            if(blockIsGrass)                            // Your condition
   *              mask[index / 64] |= 1UL << (index % 64);  // Turn on bit
   *        }
   *
   *   tree.set(mask, grassVoxel);
   *
   * @param mask   A bitmask indicating which voxels to set (1 = set, 0 = skip).
   * @param data   Pointer to the datatype to set at the marked positions.
   */
  void Set(uint64_t (&mask)[], T* data) {
    for (int z = 0; z < m_Size; z += 64)
      for (int x = 0; x < m_Size; x += 64)
        for (int y = 0; y < m_Size; y += 64)
          Set(mask, x, y, z, data, 64);
  };

  /**
   * Sets a voxel at the given 3D world position.
   *
   * @param position  The world-space position to place the voxel at.
   * @param data      The data to insert.
   * @param leafSize  The smallest voxel size (default is 1 unit).
   */
  Node* Set(glm::vec3 position, T* data, int leafSize = 1) {
    return Set(static_cast<int>(position.x), static_cast<int>(position.y), static_cast<int>(position.z), data, leafSize);
  };

  /**
   * Sets a voxel at the given 3D world position.
   *
   * @param x, y, z   The world-space position to place the voxel at.
   * @param data      The data to insert.
   * @param leafSize  The smallest voxel size (default is 1 unit).
   */
  Node* Set(int x, int y, int z, T* data, int leafSize = 1) {
    Node* root = m_Root.load(std::memory_order::acquire);
    Node* next = Set(root, x, y, z, data, leafSize, m_Size);
    m_Root.store(next, std::memory_order::release);
    return next;
  };

  /**
   * Retrieves the node at the given 3D world position.
   *
   * @param position  The world-space position to query.
   * @param nodeId    Optional filter; if provided, only matching voxels are returned.
   * @return          Pointer to the node at that position, or nullptr if not found or filtered out.
   */
  Node* Get(glm::vec3 position, uint32_t nodeId = 0) {
    return Get(static_cast<int>(position.x), static_cast<int>(position.y), static_cast<int>(position.z), nodeId);
  };

  /**
   * Retrieves the node at the given 3D world position.
   *
   * @param x, y, z   The world-space position to query.
   * @param nodeId    Optional filter; if provided, only matching voxels are returned.
   * @return          Pointer to the node at that position, or nullptr if not found or filtered out.
   */
  Node* Get(int x, int y, int z, uint32_t nodeId = 0) {
    Node* root = m_Root.load(std::memory_order::relaxed);
    return Get(root, x, y, z, m_Size, nodeId);
  };

  /**
   * Calls node.clear() on the root node.
   * This clears the root node and all it's children.
   * The root node will not be destroyed, only cleared. All children will be
   * cleared and deleted.
   *
   * @note The root node will be destroyed once the destructor of the SVO is called.
   */
  void Clear() {
    Node* root = m_Root.load(std::memory_order::acquire);

    if (!root)
      return;

    root->Clear();
  };

  /**
   * Clear a voxel at the given 3D position in voxel-space.
   * This overload uses a `glm::ivec3` for convenience.
   *
   * Internally calls the recursive clear method to remove the voxel if it
   * exists and optionally matches the given target type.
   *
   * @param position  Voxel-space coordinates to clear (x, y, z).
   * @param leafSize  Minimum size representing a leaf node; defaults to 1.
   */
  void Clear(glm::ivec3 position, int leafSize = 1) {
    Clear(position.x, position.y, position.z, leafSize);
  };

  /**
   * Clear a voxel at the given x, y, z coordinates in
   * voxel-space.
   *
   * This overload allows passing coordinates directly. It delegates to the
   * internal recursive `Clear` function.
   *
   * @param x, y, z   Voxel-space coordinates to clear.
   * @param leafSize  Minimum size representing a leaf node; defaults to 1.
   */
  void Clear(int x, int y, int z, int leafSize = 1) {
    Node* root = m_Root.load(std::memory_order::acquire);
    Node* next = Clear(root, x, y, z, leafSize, m_Size);
    m_Root.store(next, std::memory_order::release);
  };

  /**
   * Returns the total memory usage of the SVO in bytes.
   * This does not include neighbours.
   */
  size_t GetTotalMemoryUsage() {
    Node* root = m_Root.load(std::memory_order::relaxed);
    return sizeof(SparseOctree) + GetMemoryUsage(root);
  };

  /**
   * NOTE: TODO: Outdated explaination, it's a bit different. No time to update
   *
   * Flatten the tree into a vector that contains all internal nodes
   * leading up to the existing leaf nodes.
   *
   * To get the positions back from flattened data, this is the way:
   *
   * Here is some example data.
   * Voxles:
   *  #FF2D2D2D (0,0,0)
   *  #FF214365 (1,0,0)
   *  #FF228B22 (6,0,0)
   * +-------+-------+----------+-----------+----------+
   * | Index | Depth | Children | ChildIdx  | Color    |
   * +-------+-------+----------+-----------+----------+
   * |   0   |   6   | 00000001 |     1     | #00000000|
   * |   1   |   5   | 00000001 |     2     | #00000000|
   * |   2   |   4   | 00000001 |     3     | #00000000|
   * |   3   |   3   | 00010001 |     4     | #00000000|
   * |   4   |   2   | 00000001 |     5     | #00000000|
   * |   5   |   1   | 00010001 |     6     | #00000000|
   * |   6   |   0   | FF2D2D2D |     0     | #FF2D2D2D|
   * |   7   |   0   | FF214365 |     0     | #FF214365|
   * |   8   |   2   | 00010000 |     9     | #00000000|
   * |   9   |   1   | 00000001 |    10     | #00000000|
   * |  10   |   0   | FF228B22 |     0     | #FF228B22|
   * +-------+-------+----------+-----------+----------+
   *
   * How do you get the data back from this vector of structs?
   * The first thing you need is to use mortin order to find the offset
   * Here is the bitwise operations you can perform on the Children mask to get
   * the offsets
   *
   * // Where i is the index of the child (0 - 7)
   * int x = (i >> 2) & 1;
   * int y = (i >> 1) & 1;
   * int z = (i >> 0) & 1;
   *
   * You need to calculate the sum of (offset * size) until the child.
   *
   * Starting at the root, accumulate:
   *    offset += (x, y, z) * currentSize;
   *
   * Finding Voxel: #FF2D2D2D
   * +-------+-------+----------+-----------+----------+
   * | Index | Depth | Children | ChildIdx  | Color    | Size | Offset
   * +-------+-------+----------+-----------+----------+
   * |   0   |   6   | 00000001 |     1     | #00000000| 32  *  (0,0,0)
   * |   1   |   5   | 00000001 |     2     | #00000000| 16  *  (0,0,0)
   * |   2   |   4   | 00000001 |     3     | #00000000| 8   *  (0,0,0)
   * |   3   |   3   | 00010001 |     4     | #00000000| 4   *  (0,0,0)
   * |   4   |   2   | 00000001 |     5     | #00000000| 2   *  (0,0,0)
   * |   5   |   1   | 00010001 |     6     | #00000000| 1   *  (0,0,0)
   * |   6   |   0   | FF2D2D2D |     0     | #FF2D2D2D| Sum    (0,0,0)
   * |   7   |   0   | FF214365 |     0     | #FF214365|
   * |   8   |   2   | 00010000 |     9     | #00000000|
   * |   9   |   1   | 00000001 |    10     | #00000000|
   * |  10   |   0   | FF228B22 |     0     | #FF228B22|
   * +-------+-------+----------+-----------+----------+
   *
   * Finding Voxel: #FF214365
   * +-------+-------+----------+-----------+----------+
   * | Index | Depth | Children | ChildIdx  | Color    | Size | Offset
   * +-------+-------+----------+-----------+----------+
   * |   0   |   6   | 00000001 |     1     | #00000000| 32  *  (0,0,0)
   * |   1   |   5   | 00000001 |     2     | #00000000| 16  *  (0,0,0)
   * |   2   |   4   | 00000001 |     3     | #00000000| 8   *  (0,0,0)
   * |   3   |   3   | 00010001 |     4     | #00000000| 4   *  (0,0,0)
   * |   4   |   2   | 00000001 |     5     | #00000000| 2   *  (0,0,0)
   * |   5   |   1   | 00010001 |     6     | #00000000| 1   *  (1,0,0)
   * |   6   |   0   | FF2D2D2D |     0     | #FF2D2D2D|
   * |   7   |   0   | FF214365 |     0     | #FF214365| Sum    (1,0,0)
   * |   8   |   2   | 00010000 |     9     | #00000000|
   * |   9   |   1   | 00000001 |    10     | #00000000|
   * |  10   |   0   | FF228B22 |     0     | #FF228B22|
   * +-------+-------+----------+-----------+----------+
   *
   * Finding Voxel: #FF228B22
   * +-------+-------+----------+-----------+----------+
   * | Index | Depth | Children | ChildIdx  | Color    | Size | Offset
   * +-------+-------+----------+-----------+----------+
   * |   0   |   6   | 00000001 |     1     | #00000000| 32  *  (0,0,0)
   * |   1   |   5   | 00000001 |     2     | #00000000| 16  *  (0,0,0)
   * |   2   |   4   | 00000001 |     3     | #00000000| 8   *  (0,0,0)
   * |   3   |   3   | 00010001 |     4     | #00000000| 4   *  (1,0,0)
   * |   4   |   2   | 00000001 |     5     | #00000000|
   * |   5   |   1   | 00010001 |     6     | #00000000|
   * |   6   |   0   | FF2D2D2D |     0     | #FF2D2D2D|
   * |   7   |   0   | FF214365 |     0     | #FF214365|
   * |   8   |   2   | 00010000 |     9     | #00000000| 2   *  (1,0,0)
   * |   9   |   1   | 00000001 |    10     | #00000000| 1   *  (0,0,0)
   * |  10   |   0   | FF228B22 |     0     | #FF228B22| Sum    (6,0,0)
   * +-------+-------+----------+-----------+----------+
   */
  void Flatten(std::vector<FlatNode>& out) {
    Node* root = m_Root.load(std::memory_order::relaxed);

    if (!root)
      return;

    out.clear();

    m_Dirty = false;

    uint32_t index = static_cast<uint32_t>(out.size());

    out.emplace_back(
        FlatNode{.Depth = root->Depth, .Children = 0, .ChildIndex = 1});

    if (root->Data)
      out[index].Id = root->Data->Id;

    for (size_t i = 0; i < 8; i++)
      if (root->Children[i])
        out[index].Children |= (1 << i);

    Flatten(root, out);
  };

  /**
   * Returns true if the tree has changed since .Flatten() was last called
   */
  bool IsDirty() { return m_Dirty; };

  /**
   * Filter the tree for selected nodes
   *
   * @param filter Filter callback
   */
  std::vector<FilterNode> Filter(const std::function<bool(Node* node)>& filter) {
    std::vector<FilterNode> results = {};

    Node* root = m_Root.load(std::memory_order::relaxed);

    Filter(filter, results, {0, 0, 0}, m_Size, root);

    return results;
  };

  /**
   * Sets dirty to true
   */
  void Flush() { m_Dirty = true; }

  /**
   * Greedy meshes the SVO grouped by material
   */
  std::vector<Vertex> GreedyMesh(std::vector<Material> materials) {
    std::vector<std::vector<Vertex>> results(materials.size());

    auto exists = [this](float x, float y, float z, uint32_t id = 0) -> bool {
      return this->Get(x, y, z, id);
    };

    std::for_each(std::execution::par_unseq, materials.begin(), materials.end(), [this, &results, &materials, &exists](Material& material) {
      std::vector<Vertex> vertices;

      const int chunksPerAxis = std::max(1, static_cast<int>(m_Size / GreedyMesh64::CHUNK_SIZE));

      for (int cz = 0; cz < chunksPerAxis; ++cz)
        for (int cy = 0; cy < chunksPerAxis; ++cy)
          for (int cx = 0; cx < chunksPerAxis; ++cx)
            GreedyMesh64::Generate(exists, vertices, cx, cy, cz, material.Id);

      results[material.Id - 1] = std::move(vertices);
    });

    std::vector<Vertex> result = {};

    for (auto& v : results)
      result.insert(result.end(), std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()));

    return result;
  };

  /**
   * Perform a recursive raymarch from the root node of the SVO
   * Exits early, as soon as a node with a voxel is found.
   * @brief Quick raymarch
   */
  Hit Raymarch(glm::vec3 origin, glm::vec3 direction) {
    return Raymarch(m_Root, origin, direction, glm::vec3(0.), m_Size);
  };

  /**
   * Perform a recursive raymarch from the root node of the SVO
   * Does not stop until it reaches size = 1
   * @brief Slower raymarch
   */
  Hit DeepRaymarch(glm::vec3 origin, glm::vec3 direction) {
    return DeepRaymarch(m_Root, origin, direction, glm::vec3(0.), m_Size);
  };
};