#include "Node.h"

Node::Node() {}

Node::Node(uint8_t depth, VoxelType* voxel)
    : Depth(depth)
    , Voxel(voxel) {}

Node::~Node() { Clear(); }

bool Node::operator==(const Node& other) const {
  return Voxel->Material == other.Voxel->Material;
}

bool Node::operator!=(const Node& other) const { return !(*this == other); }

void Node::Clear() {
  Voxel = nullptr;

  for (int i = 0; i < 8; i++)
    if (Children[i]) {
      delete Children[i];
      Children[i] = nullptr;
    }
}