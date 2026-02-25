#pragma once

#include <atomic>
#include <cstring>
#include <glm/glm.hpp>
#include <vector>

#include "Debug.h"
#include "RCU/RCU.h"

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
template <Data T, uint8_t S = 64>
class SparseOctree {

  static_assert(S != 0 && (S & (S - 1)) == 0, "S must be a power of two");

public:
  static inline constexpr uint8_t SIZE = S;
  static inline constexpr uint8_t MOD  = SIZE - 1;
  static inline constexpr uint8_t DIV  = std::log2(SIZE);

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

  class Writer {
  private:
    std::atomic<SparseOctree<T, S>::Node*>* m_Atomic = nullptr;

  public:
    SparseOctree<T, S>::Node* Root = nullptr;

    Writer(std::atomic<SparseOctree<T, S>::Node*>* atomic)
        : m_Atomic(atomic)
        , Root(atomic->load(std::memory_order::acquire)) {}

    ~Writer() {
      m_Atomic->store(Root, std::memory_order::release);
    }
  };

  class Reader {
  private:
    RCU<Node>* m_RCU        = nullptr;
    uint64_t   m_Generation = 0;

  public:
    SparseOctree<T, S>::Node* Root = nullptr;

    Reader(std::atomic<SparseOctree<T, S>::Node*>* atomic, RCU<Node>* rcu)
        : m_RCU(rcu)
        , Root(atomic->load(std::memory_order::acquire)) {
      m_Generation = m_RCU->ReadLock();
    }

    ~Reader() {
      m_RCU->ReadUnlock(m_Generation);
    }
  };

private:
  class alignas(64) Mask {
  private:
    uint64_t m_Present[SIZE * SIZE] = {};

  public:
    Mask() = default;

    inline bool Exists(int x, int y, int z) {
      if (x < 0 || y < 0 || z < 0 || x > SIZE || y > SIZE || z > SIZE)
        return false;

      uint32_t i = x + (SIZE * (y + (SIZE * z)));
      return (m_Present[i >> DIV] >> (i & MOD)) & 1ULL;
    }

    inline bool Exists(uint8_t x, uint8_t y, uint8_t z) {
      uint32_t i = x + (SIZE * (y + (SIZE * z)));
      return (m_Present[i >> DIV] >> (i & MOD)) & 1ULL;
    }

    inline void Set(uint8_t x, uint8_t y, uint8_t z) {
      uint32_t i = x + (SIZE * (y + (SIZE * z)));
      m_Present[i >> DIV] |= (1ULL << (i & MOD));
    }

    inline void Clear(uint8_t x, uint8_t y, uint8_t z) {
      uint32_t i = x + (SIZE * (y + (SIZE * z)));
      m_Present[i >> DIV] &= ~(1ULL << (i & MOD));
    }
  };

  RCU<Node> m_RCU;

  /**
   * Pointer to the root node of the Sparse Voxel Octree.
   */
  std::atomic<Node*> m_Root = new Node(DIV);

  /**
   * A mask that can tell you if a voxel exists at x,y,z or if a voxel at x,y,z is hidden
   */
  Mask m_Mask;

private:
  Node* SetWithoutCloneCheck(Node* node, uint8_t x, uint8_t y, uint8_t z, T* data, uint8_t size) {
    if (size == 1) {
      m_RCU.Retire(node);
      node       = new Node(*node);
      node->Data = data;
      return node;
    }

    int half = size >> 1;

    int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

    int mod = half - 1;

    /**
     * If the node on this path down does not exist,
     * create a new empty one to keep traversing to size 1
     */
    if (!node->Children[index]) {
      m_RCU.Retire(node, false);
      node                  = new Node(*node);
      node->Children[index] = new Node(node->Depth - 1);
    }

    node->Children[index] = SetWithoutCloneCheck(node->Children[index], x & mod, y & mod, z & mod, data, half);

    /**
     * If all 8 children exist and are of the same voxel type.
     * Delete all 8 children and set the parent voxel as their type.
     *
     * If any child is different or does not exist,
     * no merging required, return
     */
    for (int i = 0; i < 8; i++)
      if (!node->Children[i] || !node->Children[i]->Data || node->Children[i]->Data != data)
        return node;

    /**
     * Copy before modifying
     */
    m_RCU.Retire(node);
    node = new Node(*node);

    /**
     * Merge all children
     * Retire all children, this node will represent all below
     */
    for (int i = 0; i < 8; i++)
      node->Children[i] = nullptr;

    node->Data = data;

    return node;
  };

  /**
   * Internal recursive setter that traverses and builds the tree as needed.
   * Called by the public `set(x, y, z, voxel)` and `set(vec3, voxel)` methods.
   *
   * @param node      Current node in the octree.
   * @param x,y,z     Local voxel-space coordinates at this level.
   * @tparam T        The datatype to set at the marked positions.
   * @param size      The size of the region represented by this node.
   */
  Node* Set(Node* node, uint8_t x, uint8_t y, uint8_t z, T* data, uint8_t size, bool copied = false) {
    if (size == 1) {
      if (!copied) {
        m_RCU.Retire(node);
        node = new Node(*node);
      }
      node->Data = data;
      return node;
    }

    int half = size >> 1;

    int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

    int mod = half - 1;

    /**
     * If the node on this path down does not exist,
     * create a new empty one to keep traversing to size 1
     */
    if (!node->Children[index]) {
      if (!copied) {
        m_RCU.Retire(node, false);
        node   = new Node(*node);
        copied = true;
      }
      node->Children[index] = new Node(node->Depth - 1);
    }

    node->Children[index] = Set(node->Children[index], x & mod, y & mod, z & mod, data, half, copied);

    /**
     * If all 8 children exist and are of the same voxel type.
     * Delete all 8 children and set the parent voxel as their type.
     *
     * If any child is different or does not exist,
     * no merging required, return
     */
    for (int i = 0; i < 8; i++)
      if (!node->Children[i] || !node->Children[i]->Data || node->Children[i]->Data != data)
        return node;

    /**
     * Copy before modifying
     */
    if (!copied) {
      m_RCU.Retire(node);
      node = new Node(*node);

      /**
       * Merge all children
       * Retire all children, this node will represent all below
       */
      for (int i = 0; i < 8; i++)
        node->Children[i] = nullptr;

      node->Data = data;

      return node;
    }

    node->Destroy();
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
   * @return        Pointer to the node at the target position, or nullptr if
   * not found or filtered out.
   */
  Node* Get(Node* node, uint8_t x, uint8_t y, uint8_t z, uint8_t size) {
    int half = size >> 1;

    int index = ((x >= half) << 2) | ((y >= half) << 1) | (z >= half);

    if (!node->Children[index])
      return nullptr;

    if (node->Children[index]->Data)
      return node->Children[index];

    int mod = half - 1;

    return Get(node->Children[index], x & mod, y & mod, z & mod, half);
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
    if (node->Data)
      for (int i = 0; i < 8; i++) {
        node->Children[i]       = new Node(node->Depth - 1);
        node->Children[i]->Data = node->Data;
      }

    node->Data            = nullptr;
    node->Children[index] = Clear(node->Children[index], x & mod, y & mod, z & mod, half);

    /**
     * If all children do not exist, this node must be retired
     */
    bool children = false;

    for (int i = 0; i < 8; i++)
      if (node->Children[i]) {
        children = true;
        break;
      }

    if (!children) {
      m_RCU.Retire(node);
      return nullptr;
    }

    /**
     * Early exit
     * If all children are not set, no merging will be required
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

    node->Data = first;

    return node;
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
   * RCU
   * Used for batch writing in scope
   * @note This is not thread safe, make sure only one thread is writing
   */
  Writer BeginWrite() {
    return Writer(&m_Root);
  };

  /**
   * RCU
   * Used for reading in scope
   * @note This is not thread safe, make sure only one thread is writing
   */
  Reader BeginRead() {
    return Reader(&m_Root, &m_RCU);
  };

  /**
   * RCU Sync
   */
  void Sync() { m_RCU.Sync(); }

  /**
   * Sets a voxel at the given 3D world position.
   *
   * @param x, y, z   The world-space position to place the voxel at.
   * @param data      The data to insert.
   */
  void Set(uint8_t x, uint8_t y, uint8_t z, T* data) {
    Node* root = m_Root.load(std::memory_order::acquire);
    Node* next = Set(root, x, y, z, data, SIZE);
    m_Mask.Set(x, y, z);
    m_Root.store(next, std::memory_order::release);
  };

  /**
   * Sets a voxel at the given 3D world position.
   *
   * @param x, y, z   The world-space position to place the voxel at.
   * @param data      The data to insert.
   */
  void Set(Writer& session, uint8_t x, uint8_t y, uint8_t z, T* data) {
    session.Root = Set(session.Root, x, y, z, data, SIZE);
    m_Mask.Set(x, y, z);
  };

  void SetWithoutCloneCheck(Writer& session, uint8_t x, uint8_t y, uint8_t z, T* data) {
    session.Root = SetWithoutCloneCheck(session.Root, x, y, z, data, SIZE);
    m_Mask.Set(x, y, z);
  };

  /**
   * Retrieves the node at the given 3D world position.
   *
   * @param x, y, z   The world-space position to query.
   * @return          Pointer to the node at that position, or nullptr if not found or filtered out.
   */
  Node* Get(uint8_t x, uint8_t y, uint8_t z) {
    Node* root = m_Root.load(std::memory_order::acquire);
    return Get(root, x, y, z, SIZE);
  };

  /**
   * Retrieves the node at the given 3D world position.
   *
   * @param x, y, z   The world-space position to query.
   * @return          Pointer to the node at that position, or nullptr if not found or filtered out.
   */
  Node* Get(Reader& session, uint8_t x, uint8_t y, uint8_t z) {
    return Get(session.Root, x, y, z, SIZE);
  };

  /**
   * Check if a voxel exists at location
   */
  bool Exists(int x, int y, int z) {
    return m_Mask.Exists(x, y, z);
  }

  /**
   * Check if a voxel exists at location
   */
  bool Exists(uint8_t x, uint8_t y, uint8_t z) {
    return m_Mask.Exists(x, y, z);
  }

  /**
   * Right now I check for matching block, if something changes I try to get the voxel to figure out what it is.
   * This creates more faces/verticies than skipping the match check but it greatly improves
   * the speed at which the mask is created
   * Without: if (!match) match = Get(session.Root, x, y, z, SIZE);
   * ~50ms per change
   * With: if (!match) match = Get(session.Root, x, y, z, SIZE);
   * ~0.5ms per change
   *
   * Get() is expensive and should only be done a few times a frame.
   * Cannot iterate over the entire 64x64x64 volume and call Get().
   *
   * However, just checking if a current match is there is not enough, I need
   * to know if it's a border voxel. i.e, if it's the voxel that's in contact with air.
   * i.e., possibly visible.
   */
  uint64_t (&GetAxisX(Reader& session, uint32_t nodeId))[SIZE * SIZE] {
    static thread_local uint64_t masks[SIZE * SIZE];
    std::memset(masks, 0, sizeof(masks));

    for (uint8_t z = 0; z < SIZE; z++)
      for (uint8_t y = 0; y < SIZE; y++) {
        Node* match = nullptr;

        for (uint8_t x = 0; x < SIZE; x++) {

          if (!m_Mask.Exists(x, y, z))
            continue;

          match = Get(session.Root, x, y, z, SIZE);

          if (match && match->Data->Id == nodeId) {
            const unsigned int rowIndex = x + (SIZE * (y + (SIZE * z)));
            masks[rowIndex >> DIV] |= (1ULL << (rowIndex & (SIZE - 1)));
          }
        }
      }

    return masks;
  }

  uint64_t (&GetAxisY(Reader& session, uint32_t nodeId))[SIZE * SIZE] {
    static thread_local uint64_t masks[SIZE * SIZE];
    std::memset(masks, 0, sizeof(masks));

    for (uint8_t z = 0; z < SIZE; z++)
      for (uint8_t x = 0; x < SIZE; x++) {
        Node* match = nullptr;

        for (uint8_t y = 0; y < SIZE; y++) {

          if (!m_Mask.Exists(x, y, z))
            continue;

          match = Get(session.Root, x, y, z, SIZE);

          if (match && match->Data->Id == nodeId) {
            const unsigned int columnIndex = y + (SIZE * (x + (SIZE * z)));
            masks[columnIndex >> DIV] |= (1ULL << (columnIndex & (SIZE - 1)));
          }
        }
      }

    return masks;
  }

  uint64_t (&GetAxisZ(Reader& session, uint32_t nodeId))[SIZE * SIZE] {
    static thread_local uint64_t masks[SIZE * SIZE];
    std::memset(masks, 0, sizeof(masks));

    for (uint8_t x = 0; x < SIZE; x++)
      for (uint8_t y = 0; y < SIZE; y++) {
        Node* match = nullptr;

        for (uint8_t z = 0; z < SIZE; z++) {

          if (!m_Mask.Exists(x, y, z))
            continue;

          match = Get(session.Root, x, y, z, SIZE);

          if (match && match->Data->Id == nodeId) {
            const unsigned int layerIndex = z + (SIZE * (y + (SIZE * x)));
            masks[layerIndex >> DIV] |= (1ULL << (layerIndex & (SIZE - 1)));
          }
        }
      }

    return masks;
  }

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
   * Clear a voxel at the given x, y, z coordinates in
   * voxel-space.
   *
   * This overload allows passing coordinates directly. It delegates to the
   * internal recursive `Clear` function.
   *
   * @param x, y, z   Voxel-space coordinates to clear.
   */
  void Clear(Writer& session, uint8_t x, uint8_t y, uint8_t z) {
    session.Root = Clear(session.Root, x, y, z, SIZE);
    m_Mask.Clear(x, y, z);
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

  void Flatten(Reader& session, std::vector<FlatNode>& out) {
    if (!session.Root)
      return;

    out.clear();

    uint32_t index = static_cast<uint32_t>(out.size());

    out.emplace_back(FlatNode{.PackedIDC = 0, .ChildIndex = 1});
    out[index].SetDepth(session.Root->Depth);

    if (session.Root->Data)
      out[index].SetId(session.Root->Data->Id);

    for (size_t i = 0; i < 8; i++)
      if (session.Root->Children[i])
        out[index].SetChildBit(i);

    Flatten(session.Root, out);
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

  template <typename F>
    requires FilterCallback<Node, F>
  void Filter(Reader& session, std::vector<FilterNode>& out, F&& filter) {
    Filter(out, filter, {0, 0, 0}, SIZE, session.Root);
  };

  /**
   * Perform a recursive raymarch from the root node of the SVO
   * Exits early, as soon as a node with a voxel is found.
   * @brief Quick raymarch
   */
  Hit Raymarch(const glm::vec3& origin, const glm::vec3& direction) {
    Node* root = m_Root.load(std::memory_order::acquire);
    return Raymarch(root, origin, direction, glm::vec3(0.), SIZE);
  };

  Hit Raymarch(Reader& session, const glm::vec3& origin, const glm::vec3& direction) {
    return Raymarch(session.Root, origin, direction, glm::vec3(0.), SIZE);
  };

  /**
   * Perform a recursive raymarch from the root node of the SVO
   * Does not stop until it reaches size = 1
   * @brief Slower raymarch
   */
  Hit DeepRaymarch(const glm::vec3& origin, const glm::vec3& direction) {
    Node* root = m_Root.load(std::memory_order::acquire);
    return DeepRaymarch(root, origin, direction, glm::vec3(0.), SIZE);
  };

  Hit DeepRaymarch(Reader& session, const glm::vec3& origin, const glm::vec3& direction) {
    return DeepRaymarch(session.Root, origin, direction, glm::vec3(0.), SIZE);
  };
};