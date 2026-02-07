#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <glm/glm.hpp>
#include <new>
#include <vector>

#include "RCU/RCU.h"
#include "Utility/Debug.h"

template <typename T>
concept Data = requires(T t) {
  requires std::same_as<decltype(t.Id), uint32_t>;
};

template <typename N, typename F>
concept FilterCallback = requires(F f, const N* node) {
  { f(node) } -> std::same_as<bool>;
};

/**
 * @tparam T The datatype of the pointer stored
 * @tparam S The size of the SVO
 */
template <Data T>
class SparseOctree {

public:
  static constexpr uint8_t SIZE = 64;

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
    Node*   Children[8] = {nullptr};

    Node() = default;

    Node(uint8_t depth, T* data = nullptr)
        : Depth(depth)
        , Data(data){};

    bool operator==(const Node& other) const { return Data.Id == other.Data.Id; };
    bool operator!=(const Node& other) const { return Data.Id != other.Data.Id; };

    void Destroy() {
      Data = nullptr;

      for (int i = 0; i < 8; i++)
        if (Children[i]) {
          Children[i]->Destroy();
          delete Children[i];
          Children[i] = nullptr;
        }
    };
  };

  struct FlatNode {
    // 16bit Id, 8bit Depth & Children
    uint32_t PackedIDC  = 0;
    uint32_t ChildIndex = 0;

    inline void SetId(uint16_t id) {
      PackedIDC = (PackedIDC & 0x0000FFFFu) | (uint32_t(id) << 16);
    }

    inline void SetDepth(uint8_t depth) {
      PackedIDC = (PackedIDC & 0xFFFF00FFu) | (uint32_t(depth) << 8);
    }

    inline void SetChildBit(uint8_t index) {
      PackedIDC |= (1u << index);
    }

    inline uint16_t GetId() const {
      return uint16_t(PackedIDC >> 16);
    }

    inline uint8_t GetDepth() const {
      return uint8_t((PackedIDC >> 8) & 0xFF);
    }

    inline uint8_t GetChildren() const {
      return uint8_t(PackedIDC & 0xFF);
    }
  };

  struct FilterNode {
    uint32_t  Id    = 0;
    uint32_t  Depth = 0;
    uint32_t  P1[2];
    glm::vec3 Position = glm::vec3(0.0f);
    uint32_t  P2[1];
  };

private:
  struct Mask {
    /**
     * 0 - Air
     * 1 - Voxel
     */
    uint64_t m_Present[SIZE * SIZE];

    /**
     * 0 - Exposed
     * 1 - Hidden (inside walls, underground, etc)
     */
    uint64_t m_Hidden[SIZE * SIZE];

    inline bool Exists(uint8_t x, uint8_t y, uint8_t z) {
      uint32_t i = x + (SIZE * (y + (SIZE * z)));
      return (m_Present[i >> 6] >> (i & 63)) & 1ULL;
    }

    inline bool Hidden(uint8_t x, uint8_t y, uint8_t z) {
      uint32_t i = x + (SIZE * (y + (SIZE * z)));
      return (m_Hidden[i >> 6] >> (i & 63)) & 1ULL;
    }

    inline bool Present(int x, int y, int z) {
      if (x < 0 || y < 0 || z < 0 || x >= SIZE || y >= SIZE || z >= SIZE)
        return false;

      uint32_t i = x + (SIZE * (y + (SIZE * z)));
      return (m_Present[i >> 6] >> (i & 63)) & 1ULL;
    }

    inline void ComputeHidden(uint8_t x, uint8_t y, uint8_t z) {
      uint32_t i    = x + SIZE * (y + SIZE * z);
      uint32_t word = i >> 6;
      uint64_t mask = 1ULL << (i & 63);

      if (!((m_Present[word] >> (i & 63)) & 1ULL)) {
        m_Hidden[word] &= ~mask;
        return;
      }

      static constexpr int dx[6] = {-1, 1, 0, 0, 0, 0};
      static constexpr int dy[6] = {0, 0, -1, 1, 0, 0};
      static constexpr int dz[6] = {0, 0, 0, 0, -1, 1};

      for (int n = 0; n < 6; n++)
        if (!Present(x + dx[n], y + dy[n], z + dz[n])) {
          m_Hidden[word] &= ~mask;
          return;
        }

      m_Hidden[word] |= mask;
    }

    inline void Set(uint8_t x, uint8_t y, uint8_t z) {
      uint32_t i = x + (SIZE * (y + (SIZE * z)));
      m_Present[i >> 6] |= (1ULL << (i & 63));

      static constexpr int dx[6] = {-1, 1, 0, 0, 0, 0};
      static constexpr int dy[6] = {0, 0, -1, 1, 0, 0};
      static constexpr int dz[6] = {0, 0, 0, 0, -1, 1};

      ComputeHidden(x, y, z);

      for (int n = 0; n < 6; n++) {
        int nx = int(x) + dx[n];
        int ny = int(y) + dy[n];
        int nz = int(z) + dz[n];

        if (nx < 0 || ny < 0 || nz < 0 || nx >= SIZE || ny >= SIZE || nz >= SIZE)
          continue;

        ComputeHidden(nx, ny, nz);
      }
    }

    inline void Clear(uint8_t x, uint8_t y, uint8_t z) {
      uint32_t i = x + (SIZE * (y + (SIZE * x)));
      m_Present[i >> 6] &= ~(1ULL << (i & 63));
      m_Hidden[i >> 6] &= ~(1ULL << (i & 63));

      static constexpr int dx[6] = {-1, 1, 0, 0, 0, 0};
      static constexpr int dy[6] = {0, 0, -1, 1, 0, 0};
      static constexpr int dz[6] = {0, 0, 0, 0, -1, 1};

      for (int n = 0; n < 6; n++) {
        int nx = int(x) + dx[n];
        int ny = int(y) + dy[n];
        int nz = int(z) + dz[n];

        if (nx < 0 || ny < 0 || nz < 0 || nx >= SIZE || ny >= SIZE || nz >= SIZE)
          continue;

        ComputeHidden(nx, ny, nz);
      }
    }
  };

  RCU<Node> m_RCU;

  /**
   * Pointer to the root node of the Sparse Voxel Octree.
   */
  std::atomic<Node*> m_Root = new Node(6);

  /**
   * Tracks wheather the tree has changed.
   */
  bool m_Dirty = false;

  /**
   * On Flush callback
   */
  std::function<void()> m_Flush;

  /**
   * A mask that can tell you if a voxel exists at x,y,z or if a voxel at x,y,z is hidden
   */
  Mask m_Mask;

private:
  /**
   * Internal recursive setter that traverses and builds the tree as needed.
   * Called by the public `set(x, y, z, voxel)` and `set(vec3, voxel)` methods.
   *
   * @param node      Current node in the octree.
   * @param x,y,z     Local voxel-space coordinates at this level.
   * @tparam T        The datatype to set at the marked positions.
   * @param size      The size of the region represented by this node.
   */
  Node* Set(Node* node, uint8_t x, uint8_t y, uint8_t z, T* data, uint8_t size) {
    m_RCU.Retire(node, false);

    node = new Node(*node);

    if (size == 1) {
      node->Data = data;
      m_Dirty    = true;
      return node;
    }

    int half = size >> 1;

    int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

    int mod = half - 1;

    /**
     * If the node on this path down does not exist,
     * create a new empty one to keep traversing to size 1
     */
    if (!node->Children[index])
      node->Children[index] = new Node(static_cast<uint8_t>(std::log2(half)));

    node->Children[index] = Set(node->Children[index], x & mod, y & mod, z & mod, data, half);

    if (node->Data)
      node->Data = nullptr;

    /**
     * Early exit
     * If there are no children, no merging will be required
     */
    if (!node->Children[0] || !node->Children[0]->Data)
      return node;

    /**
     * If all 8 children exist and are of the same voxel type.
     * Delete all 8 children and set the parent voxel as their type.
     */
    T* first = node->Children[0]->Data;

    /**
     * If any child is different or does not exist,
     * no merging required, return
     */
    for (int i = 0; i < 8; i++)
      if (!node->Children[i] || !node->Children[i]->Data || node->Children[i]->Data != first)
        return node;

    /**
     * Merge all children
     * Retire all children, this node will represent all below
     */
    for (int i = 0; i < 8; i++) {
      m_RCU.Retire(node->Children[i]);
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
  Node* Get(Node* node, uint8_t x, uint8_t y, uint8_t z, uint32_t nodeId, uint8_t size) {
    if (!node)
      return nullptr;

    if (node->Data) {
      if (nodeId && nodeId != node->Data->Id)
        return nullptr;
      return node;
    }

    if (size == 1)
      return nullptr;

    int half = size >> 1;

    int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

    int mod = half - 1;

    return Get(node->Children[index], x & mod, y & mod, z & mod, nodeId, half);
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
   * @param size      The size of the region represented by this node.
   */
  Node* Clear(Node* node, uint8_t x, uint8_t y, uint8_t z, uint8_t size) {
    if (!node)
      return nullptr;

    if (size == 1) {
      m_RCU.Retire(node);
      m_Dirty = true;
      return nullptr;
    }

    m_RCU.Retire(node, false);

    node = new Node(*node);

    int half = size >> 1;

    int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

    int mod = half - 1;

    /**
     * If this node has data, it's children were merged
     * Create all children with the same data & clear the parent
     */
    if (node->Data) {
      for (int i = 0; i < 8; i++) {
        node->Children[i]       = new Node(static_cast<uint8_t>(std::log2(half)));
        node->Children[i]->Data = node->Data;
      }
      node->Data = nullptr;
    }

    node->Children[index] = Clear(node->Children[index], x & mod, y & mod, z & mod, half);

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

      out.emplace_back(FlatNode{.PackedIDC = 0, .ChildIndex = 0});
      out[index].SetDepth(child->Depth);

      if (child->Data)
        out[index].SetId(child->Data->Id);

      for (size_t i = 0; i < 8; i++)
        if (child->Children[i])
          out[index].SetChildBit(i);
    }

    uint32_t pIndex = static_cast<uint32_t>(out.size()) - 1;

    for (int i = 0; i < 8; i++) {
      if (!node->Children[i])
        continue;

      uint32_t nIndex = pIndex - childCount--;

      if (out[nIndex].GetDepth())
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
  template <typename F>
    requires FilterCallback<Node, F>
  void Filter(std::vector<FilterNode>& out, F&& filter, const glm::vec3& position, uint8_t size, Node* node) {
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

    int half = size >> 1;

    for (size_t i = 0; i < 8; i++) {
      if (!node->Children[i])
        continue;

      glm::vec3 offset = glm::vec3((i >> 2) & 1, (i >> 1) & 1, (i >> 0) & 1);

      glm::vec3 childMin = position + offset * static_cast<float>(half);

      Filter(out, filter, childMin, half, node->Children[i]);
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

    float half = size >> 1;

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

    float half = size >> 1;

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
  SparseOctree() = default;

  /**
   * Destroys the Sparse Voxel Octree and frees all allocated nodes.
   */
  ~SparseOctree() {
    Node* root = m_Root.load(std::memory_order::acquire);
    root->Destroy();
    delete root;
  };

  /**
   * Returns the total size (width, height, depth) of the root node.
   *
   * @return The root node's spatial size (e.g., 256).
   */
  uint32_t GetSize() { return SIZE; };

  /**
   * Sets a voxel at the given 3D world position.
   *
   * @param position  The world-space position to place the voxel at.
   * @param data      The data to insert.
   */
  Node* Set(const glm::vec3& position, T* data) {
    return Set(static_cast<int>(position.x), static_cast<int>(position.y), static_cast<int>(position.z), data);
  };

  /**
   * Sets a voxel at the given 3D world position.
   *
   * @param x, y, z   The world-space position to place the voxel at.
   * @param data      The data to insert.
   */
  Node* Set(uint8_t x, uint8_t y, uint8_t z, T* data) {
    Node* root = m_Root.load(std::memory_order::acquire);
    Node* next = Set(root, x, y, z, data, SIZE);
    m_Mask.Set(x, y, z);
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
  Node* Get(const glm::vec3& position, uint32_t nodeId = 0) {
    return Get(static_cast<int>(position.x), static_cast<int>(position.y), static_cast<int>(position.z), nodeId);
  };

  /**
   * Retrieves the node at the given 3D world position.
   *
   * @param x, y, z   The world-space position to query.
   * @param nodeId    Optional filter; if provided, only matching voxels are returned.
   * @return          Pointer to the node at that position, or nullptr if not found or filtered out.
   */
  Node* Get(uint8_t x, uint8_t y, uint8_t z, uint32_t nodeId = 0) {
    Node* root = m_Root.load(std::memory_order::acquire);
    return Get(root, x, y, z, nodeId, SIZE);
  };

  /**
   * Retrieves the node at the given 3D world position.
   *
   * @param x, y, z   The world-space position to query.
   * @param nodeId    Optional filter; if provided, only matching voxels are returned.
   * @return          Pointer to the node at that position, or nullptr if not found or filtered out.
   */
  Node* Get(Node* root, uint8_t x, uint8_t y, uint8_t z, uint32_t nodeId = 0) {
    return Get(root, x, y, z, nodeId, SIZE);
  };

  /**
   * Check if a voxel exists at location
   */
  bool Exists(uint8_t x, uint8_t y, uint8_t z) {
    return m_Mask.Exists(x, y, z);
  }

  /**
   * Check if a voxel is hidden (behind other voxels)
   */
  bool Hidden(uint8_t x, uint8_t y, uint8_t z) {
    return m_Mask.Hidden(x, y, z);
  }

  Node* GetRoot(std::memory_order order) {
    return m_Root.load(order);
  }

  /**
   * I only care about voxels that face the air, voxels that are under ground, or in walls,
   * it doesn't matter what material they are
   * what I need is
   * 000122221000
   * 0 - false
   * 1 - check
   * 2 - true
   *
   * This way I can skip many gets
   * hence I will store a uint128_t mask[4096]
   */
  uint64_t (&GetAxisX(Node* root, uint32_t nodeId))[4096] {
    static thread_local uint64_t masks[4096];
    std::memset(masks, 0, sizeof(masks));

    for (uint8_t z = 0; z < SIZE; z++)
      for (uint8_t y = 0; y < SIZE; y++) {

        // bool match = false;

        for (uint8_t x = 0; x < SIZE; x++) {

          // if (!m_Mask.Exists(x, y, z))
          //   continue;

          // if (!m_Mask.Hidden(x, y, z))
          //   match = !!Get(root, x, y, z, nodeId, SIZE);

          if (Get(root, x, y, z, nodeId, SIZE)) {
            const unsigned int rowIndex = x + (SIZE * (y + (SIZE * z)));
            masks[rowIndex >> 6] |= (1ULL << (rowIndex & (SIZE - 1)));
          }
        }
      }

    return masks;
  }

  uint64_t (&GetAxisY(Node* root, uint32_t nodeId))[4096] {
    static thread_local uint64_t masks[4096];
    std::memset(masks, 0, sizeof(masks));

    for (uint8_t z = 0; z < SIZE; z++)
      for (uint8_t x = 0; x < SIZE; x++) {
        // bool match = false;

        for (uint8_t y = 0; y < SIZE; y++) {

          // if (!m_Mask.Exists(x, y, z))
          //   continue;

          // if (!m_Mask.Hidden(x, y, z))
          //   match = !!Get(root, x, y, z, nodeId, SIZE);

          if (Get(root, x, y, z, nodeId, SIZE)) {
            const unsigned int columnIndex = y + (SIZE * (x + (SIZE * z)));
            masks[columnIndex >> 6] |= (1ULL << (columnIndex & (SIZE - 1)));
          }
        }
      }

    return masks;
  }

  uint64_t (&GetAxisZ(Node* root, uint32_t nodeId))[4096] {
    static thread_local uint64_t masks[4096];
    std::memset(masks, 0, sizeof(masks));

    for (uint8_t x = 0; x < SIZE; x++)
      for (uint8_t y = 0; y < SIZE; y++) {
        // bool match = false;

        for (uint8_t z = 0; z < SIZE; z++) {

          // if (!m_Mask.Exists(x, y, z))
          //   continue;

          // if (!m_Mask.Hidden(x, y, z))
          //   match = !!Get(root, x, y, z, nodeId, SIZE);

          if (Get(root, x, y, z, nodeId, SIZE)) {
            const unsigned int layerIndex = z + (SIZE * (y + (SIZE * x)));
            masks[layerIndex >> 6] |= (1ULL << (layerIndex & (SIZE - 1)));
          }
        }
      }

    return masks;
  }

  /**
   * Clear a voxel at the given 3D position in voxel-space.
   * This overload uses a `glm::ivec3` for convenience.
   *
   * Internally calls the recursive clear method to remove the voxel if it
   * exists and optionally matches the given target type.
   *
   * @param position  Voxel-space coordinates to clear (x, y, z).
   */
  void Clear(const glm::ivec3& position) {
    Clear(position.x, position.y, position.z);
  };

  /**
   * Clear a voxel at the given x, y, z coordinates in
   * voxel-space.
   *
   * This overload allows passing coordinates directly. It delegates to the
   * internal recursive `Clear` function.
   *
   * @param x, y, z   Voxel-space coordinates to clear.
   */
  void Clear(uint8_t x, uint8_t y, uint8_t z) {
    Node* root = m_Root.load(std::memory_order::acquire);
    Node* next = Clear(root, x, y, z, SIZE);
    m_Mask.Clear(x, y, z);
    m_Root.store(next, std::memory_order::release);
  };

  /**
   * Returns the total memory usage of the SVO in bytes.
   * This does not include neighbours.
   */
  size_t GetTotalMemoryUsage() {
    Node* root = m_Root.load(std::memory_order::acquire);
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
   * +-------+-------+----------+-----------+------------+
   * | Index | Depth | Children | ChildIdx  | Color      |
   * +-------+-------+----------+-----------+------------+
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
   * +-------+-------+----------+-----------+------------+
   *
   * How do you get the data back from this vector of structs?
   * The first thing you need is to use mortin order to find the offset
   * Here is the bitwise operations you can perform on the Children mask to get
   * the offsets
   *
   * // Where i is the index of the child (0 - 7)
   * uint8_t x = (i >> 2) & 1;
   * uint8_t y = (i >> 1) & 1;
   * uint8_t z = (i >> 0) & 1;
   *
   * You need to calculate the sum of (offset * size) until the child.
   *
   * Starting at the root, accumulate:
   *    offset += (x, y, z) * currentSize;
   *
   * Finding Voxel: #FF2D2D2D
   * +-------+-------+----------+-----------+------------+
   * | Index | Depth | Children | ChildIdx  | Color      | Size | Offset
   * +-------+-------+----------+-----------+------------+
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
   * +-------+-------+----------+-----------+------------+
   */
  void Flatten(std::vector<FlatNode>& out) {
    Node* root = m_Root.load(std::memory_order::acquire);

    if (!root)
      return;

    out.clear();

    uint32_t index = static_cast<uint32_t>(out.size());

    out.emplace_back(FlatNode{.PackedIDC = 0, .ChildIndex = 1});
    out[index].SetDepth(root->Depth);

    if (root->Data)
      out[index].SetId(root->Data->Id);

    for (size_t i = 0; i < 8; i++)
      if (root->Children[i])
        out[index].SetChildBit(i);

    Flatten(root, out);
  };

  /**
   * Filter the tree for selected nodes
   *
   * @param filter Filter callback
   */
  template <typename F>
    requires FilterCallback<Node, F>
  void Filter(std::vector<FilterNode>& out, F&& filter) {
    Node* root = m_Root.load(std::memory_order::acquire);
    Filter(out, filter, {0, 0, 0}, SIZE, root);
  };

  /**
   * Sets dirty to true
   */
  void Flush() {
    m_Dirty = true;
    if (m_Flush)
      m_Flush();
  }

  /**
   * Sets dirty to false
   */
  void Clean() { m_Dirty = false; }

  /**
   * Returns true if the tree is dirty
   */
  bool IsDirty() { return m_Dirty; };

  /**
   * On flush callback
   */
  void OnFlush(std::function<void()> callback) { m_Flush = callback; };

  /**
   * Perform a recursive raymarch from the root node of the SVO
   * Exits early, as soon as a node with a voxel is found.
   * @brief Quick raymarch
   */
  Hit Raymarch(const glm::vec3& origin, const glm::vec3& direction) {
    return Raymarch(m_Root, origin, direction, glm::vec3(0.), SIZE);
  };

  /**
   * Perform a recursive raymarch from the root node of the SVO
   * Does not stop until it reaches size = 1
   * @brief Slower raymarch
   */
  Hit DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction) {
    return DeepRaymarch(m_Root, origin, direction, glm::vec3(0.), SIZE);
  };

  /**
   * RCU Sync
   */
  void Sync() { m_RCU.Sync(); }

  /**
   * RCU ReadLock
   */
  uint64_t ReadLock() { return m_RCU.ReadLock(); }

  /**
   * RCU ReadUnlock
   */
  void ReadUnlock(uint64_t generation) { return m_RCU.ReadUnlock(generation); }
};