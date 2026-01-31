#pragma once

#include <atomic>
#include <glm/glm.hpp>
#include <vector>

#include "Utility/Debug.h"

#include "Voxel/Node.h"
#include "Voxel/Type.h"
#include "Voxel/Voxel.h"

class SparseVoxelOctree {

  using TVoxel = Voxel;

public:
  /**
   * The hit struct for raymarching
   */
  struct Hit {
    float     TMin     = 0.0f;
    glm::vec3 Position = glm::vec3(0.);
    glm::vec3 Normal   = glm::vec3(0.);
    TVoxel*   Voxel    = nullptr;
    uint32_t  Size     = 0;
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
   * The coordinate of this SVO chunk in the global grid of chunks.
   * Used for resolving neighbor lookups.
   *
   * Example:
   *
   *   (0,0,0) => center
   *   (1,0,0) => center right
   *   (-1,0,0) => center left
   */
  glm::ivec3 m_ChunkCoord{0, 0, 0};

  /**
   * A flattened representation of the SVO that can be sent to the
   * GPU
   */
  std::vector<FlatVoxel> m_Flat = {};

  /**
   * Tracks wheather the tree has changed. It's reset to false once Flatten is
   * called.
   */
  bool m_Dirty = false;

  /**
   * The current number of DDGI Probes
   */
  uint32_t m_DDGIProbeSize = 0;

  /**
   * VoidNode
   * i.e, the nodes outside of the world, if there is no neighbouring chunk
   */
  Node m_Void = Node(0, new Voxel());

private:
  /**
   * Internal recursive setter that applies a voxel to all positions marked in
   * the bitmask. Called by the public `set(mask, voxel)` method.
   *
   * @param mask   A bitmask indicating which voxels to set.
   * @param x,y,z  The origin (offset) position in voxel-space for this mask
   * block.
   * @param voxel  The voxel type to set at the marked positions.
   * @param size   The current size of the region being processed.
   */
  void Set(uint64_t (&mask)[], int x, int y, int z, Voxel* voxel, int size);

  /**
   * Internal recursive setter that traverses and builds the tree as needed.
   * Called by the public `set(x, y, z, voxel)` and `set(vec3, voxel)` methods.
   *
   * @param node      Current node in the octree.
   * @param x,y,z     Local voxel-space coordinates at this level.
   * @param voxel     The voxel to assign at the final leaf.
   * @param leafSize  The target size of a leaf node (typically 1).
   * @param size      The size of the region represented by this node.
   *
   * @return          True if a voxel was set
   */
  Node* Set(Node* node, int x, int y, int z, Voxel* voxel, int leafSize, int size);

  /**
   * Internal recursive getter that traverses the tree to find a voxel at the
   * given position. Called by the public `get(x, y, z)` and `get(vec3)`
   * methods.
   *
   * @param node    Current node in the octree.
   * @param x,y,z   Local voxel-space coordinates at this level.
   * @param size    The size of the region represented by this node.
   * @param filter  Optional filter; only returns nodes matching this voxel
   * type.
   * @return        Pointer to the node at the target position, or nullptr if
   * not found or filtered out.
   */
  Node* Get(Node* node, int x, int y, int z, int size, uint32_t material = 0);

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
   * @param leafSize  Minimum size representing a leaf node; used to terminate
   *                  recursion.
   * @param size      The size of the region represented by this node.
   * @param target    Optional voxel filter; only deletes a voxel if it matches
   *                  this value. If nullptr, deletes unconditionally.
   *
   * @return          The node
   */
  Node* Clear(Node*& node, int x, int y, int z, int leafSize, int size, Voxel* target = nullptr);

  /**
   * Returns the total memory used in bytes by this node and all it's children.
   */
  size_t GetMemoryUsage(Node* node);

  /**
   * Flatten the tree into a vector that contains all internal nodes
   * leading up to the existing leaf nodes.
   *
   * @param node A pointer to the node to start flattening from
   */
  void Flatten(Node* node);

  /**
   * Filter the tree for selected nodes
   *
   * @param vector Out vector
   * @param filter Filter callback
   * @param position Position of the node
   * @param size Size of the node
   * @param node Pointer to the node from which to start a recursive search
   */
  void Filter(std::vector<DenseVoxel>&               vector,
              const std::function<bool(Node* node)>& filter,
              glm::vec3                              position,
              int                                    size,
              Node*                                  node);

  /**
   * @returns An average of all voxels at this node's position
   */
  Voxel AverageVoxel(Node* node);

  /**
   * Traverses the octree starting from the given node and applies a callback
   * function to each node in the subtree.
   *
   * @param callback The function to apply to each node.
   * @param position The world-space position of the node's minimum corner.
   * @param size The size of the node's bounding cube.
   * @param node The current octree node to process.
   */
  void TraverseNodes(std::function<void(Node* node)> callback,
                     glm::vec3                       position,
                     int                             size,
                     Node*                           node);

  /**
   * Recursively performs frustum culling starting from the given node.
   * Marks nodes as culled or visible depending on their relation to the
   * frustum.
   *
   * @param frustum The view frustum to test against.
   * @param position The world-space position of the node's minimum corner.
   * @param size The size of the node's bounding cube.
   * @param node The current octree node to process.
   */
  void FrustumCull(const Frustum& frustum, glm::vec3 position, int size, Node* node);

  /**
   * Perform an AABB intersection test
   */
  bool intersectAABB(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& min, const glm::vec3& max, float& tMin, float& tMax, glm::vec3& outNormal);

  /**
   * Perform a recursive raymarch on the SVO
   */
  Hit Raymarch(Node* node, const glm::vec3& origin, const glm::vec3& direction, glm::vec3 nodeMin, uint32_t size);

  /**
   * Perform a recursive raymarch on the SVO
   */
  Hit DeepRaymarch(Node* node, const glm::vec3& origin, const glm::vec3& direction, glm::vec3 nodeMin, uint32_t size, Voxel* voxel = nullptr);

public:
  /**
   * Constructs an empty Sparse Voxel Octree with default settings.
   */
  SparseVoxelOctree();

  /**
   * Constructs a Sparse Voxel Octree with the given spatial size.
   *
   * @param size  The side length of the root node's region (e.g., 256 for a
   * 256³ volume).
   */
  SparseVoxelOctree(uint32_t size);

  /**
   * Destroys the Sparse Voxel Octree and frees all allocated nodes.
   */
  ~SparseVoxelOctree();

  /**
   * Returns the total size (width, height, depth) of the root node.
   *
   * @return The root node's spatial size (e.g., 256).
   */
  uint32_t GetSize();

  /**
   * Returns the maximum depth of the octree.
   * This is computed based on the size: std::log2(size)
   *
   * @return Depth of the tree in number of subdivisions.
   */
  uint32_t GetDepth();

  /**
   * Returns a pointer to the root node of the octree.
   *
   * @return Root node of the SVO.
   */
  Node* GetRoot();

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
   * @param voxel  Pointer to the voxel type to set at the marked positions.
   */
  void Set(uint64_t (&mask)[], Voxel* voxel);

  /**
   * Sets a voxel at the given 3D world position.
   *
   * @param position  The world-space position to place the voxel at.
   * @param voxel     The voxel data to insert.
   * @param leafSize  The smallest voxel size (default is 1 unit).
   * @return          True if a voxel was set
   */
  Node* Set(glm::vec3 position, Voxel* voxel, int leafSize = 1);

  /**
   * Sets a voxel at the given 3D world position.
   *
   * @param x, y, z   The world-space position to place the voxel at.
   * @param voxel     The voxel data to insert.
   * @param leafSize  The smallest voxel size (default is 1 unit).
   * @return          True if a voxel was set
   */
  Node* Set(int x, int y, int z, Voxel* voxel, int leafSize = 1);

  /**
   * Retrieves the node at the given 3D world position.
   *
   * @param position  The world-space position to query.
   * @param filter    Optional filter; if provided, only matching voxels are
   * returned.
   * @return          Pointer to the node at that position, or nullptr if not
   * found or filtered out.
   */
  Node* Get(glm::vec3 position, uint32_t material = 0);

  /**
   * Retrieves the node at the given 3D world position.
   *
   * @param x, y, z   The world-space position to query.
   * @param filter    Optional filter; if provided, only matching voxels are
   * returned.
   * @return          Pointer to the node at that position, or nullptr if not
   * found or filtered out.
   */
  Node* Get(int x, int y, int z, uint32_t material = 0);

  /**
   * Calls node.clear() on the root node.
   * This clears the root node and all it's children.
   * The root node will not be destroyed, only cleared. All children will be
   * cleared and deleted.alignas The root node will be destroyed once the
   * destructor of the SVO is called.
   */
  void Clear();

  /**
   * Clear a voxel at the given 3D position in voxel-space.
   * This overload uses a `glm::ivec3` for convenience.
   *
   * Internally calls the recursive clear method to remove the voxel if it
   * exists and optionally matches the given target type.
   *
   * @param position  Voxel-space coordinates to clear (x, y, z).
   * @param leafSize  Minimum size representing a leaf node; defaults to 1.
   * @param target    Optional voxel filter; only deletes if matching this.
   */
  void Clear(glm::ivec3 position, int leafSize = 1, Voxel* target = nullptr);

  /**
   * Clear a voxel at the given x, y, z coordinates in
   * voxel-space.
   *
   * This overload allows passing coordinates directly. It delegates to the
   * internal recursive `Clear` function.
   *
   * @param x, y, z   Voxel-space coordinates to clear.
   * @param leafSize  Minimum size representing a leaf node; defaults to 1.
   * @param target    Optional voxel filter; only deletes if matching this.
   */
  void Clear(int x, int y, int z, int leafSize = 1, Voxel* target = nullptr);

  /**
   * Returns the total memory usage of the SVO in bytes.
   * This does not include neighbours.
   */
  size_t GetTotalMemoryUsage();

  /**
   * NOTE: Outdated
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
  std::vector<FlatVoxel>& Flatten();

  /**
   * Returns the flattened tree for raytracing, etc.
   */
  std::vector<FlatVoxel>& GetFlatTree();

  /**
   * Returns true if the tree has changed since .Flatten() was last called
   */
  bool IsDirty() { return m_Dirty; };

  /**
   * Filter the tree for selected nodes
   *
   * @param filter Filter callback
   */
  std::vector<DenseVoxel> Filter(const std::function<bool(Node* node)>& filter);

  /**
   * Returns a dense voxel grid of mipmaps in a flat vector
   */
  std::vector<uint32_t> GenerateMipmaps();

  /**
   * Fill a vector with a dense voxel grid.
   * @param out The vector to fill
   * @param offset The offset index to start inserting into
   * @param mipSize The mipmap size i.e 0,1,2...std::pow(2, depth)
   * @param position The position to start from, start at vec3(0.0f)
   * @param size The size of the current child at position x, starts at m_Size
   * @param node The node to start recursively checking, starts at m_Root
   */
  void FillDenseMipmap(std::vector<uint32_t>& out, uint32_t offset, uint32_t mipSize, glm::vec3 position, int size, Node* node);

  /**
   * Computes the best possible positions for each DDGI probe.
   * @param size The cell size for each probe, each probe will be positioned in
   * the center of the cell. Must be in multiples of 8.
   */
  std::vector<DDGIProbe> GenerateDDGIProbes(uint32_t size);

  void GenerateDDGIProbes(std::vector<DDGIProbe>& probes, uint32_t targetDepth, glm::vec3 position, int size, Node* node);

  /**
   * Rounds up to the nearest power of 2
   */
  static uint32_t RoundUpToPowerOfTwo(uint32_t n);

  /**
   * Computes the DDGI Probe Atlas size.
   * @param count The number of probes
   * @param texSize The size of the texture for each probe. 6,8,16
   */
  static std::tuple<uint32_t, uint32_t>
  ComputeDDGIAtlasSize(uint32_t count, uint32_t texSize, uint32_t& tilesPerRow, uint32_t& tilesPerCol);

  /**
   * Returns the size of the last computed DDGI probe vector.
   */
  uint32_t GetDDGIProbeSize() { return m_DDGIProbeSize; };

  /**
   * Sets dirty to true
   */
  void Flush() { m_Dirty = true; }

  /**
   * Greedy meshes the SVO grouped by material
   */
  std::vector<Vertex> GreedyMesh(std::vector<Material> materials);

  /**
   * Performs frustum culling on the entire octree from the root node.
   *
   * @param frustum The view frustum to test against.
   */
  void FrustumCull(const Frustum& frustum);

  /**
   * Perform a recursive raymarch from the root node of the SVO
   * Exits early, as soon as a node with a voxel is found.
   * @brief Quick raymarch
   */
  Hit Raymarch(glm::vec3 origin, glm::vec3 direction);

  /**
   * Perform a recursive raymarch from the root node of the SVO
   * Does not stop until it reaches size = 1
   * @brief Slower raymarch
   */
  Hit DeepRaymarch(glm::vec3 origin, glm::vec3 direction);
};