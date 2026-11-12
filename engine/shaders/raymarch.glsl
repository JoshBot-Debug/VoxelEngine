struct SVONode {
  uint PackedIDC;
  uint ChildIndex;
};

struct StackEntry {
  float TMin;
  uint  Index;
  vec3  Min;
};

struct VoxelsHeader {
  uint Id;
  uint Offset;
  uint Size;
};

layout(std430, set = 1, binding = 54) readonly buffer ChunkOctree {
  uint    count;
  uint    padding[3];
  SVONode data[];
}
chunks;

layout(std430, set = 1, binding = 50) readonly buffer VoxelOctrees {
  SVONode data[];
}
voxels;

layout(std430, set = 1, binding = 58) readonly buffer VoxelOctreeHeaders {
  uint         count;
  uint         padding[3];
  VoxelsHeader data[];
}
headers;

const uint  MAX_STACK = 64;
const float SIZE      = 64.0;

bool intersect(vec3 ro, vec3 rd, vec3 boxMin, vec3 boxMax, out float tmin) {
  tmin = -1e30;
  float tmax = 1e30;

  for (int i = 0; i < 3; i++) {
    if (rd[i] == 0.0) {
      if (ro[i] < boxMin[i] || ro[i] > boxMax[i])
        return false;

      continue;
    }

    float invD = 1.0 / rd[i];

    float t0 = (boxMin[i] - ro[i]) * invD;
    float t1 = (boxMax[i] - ro[i]) * invD;

    if (t0 > t1) {
      float temp = t0;
      t0         = t1;
      t1         = temp;
    }

    tmin = max(tmin, t0);
    tmax = min(tmax, t1);

    if (tmin > tmax)
      return false;
  }

  if (tmax < 0.0)
    return false;

  tmin = max(tmin, 0.0);

  return true;
}

Hit raymarchChunks(vec3 origin, vec3 direction, float dist) {
  Hit payload;
  payload.IsValid = false;
  payload.TMin    = 0.;

  StackEntry stack[MAX_STACK];

  uint stackPtr     = 0;
  stack[stackPtr++] = StackEntry(0., 0, vec3(0.));

  uint order[8];

  while (stackPtr > 0) {
    StackEntry entry = stack[--stackPtr];

    SVONode voxel = chunks.data[entry.Index];

    uint depth = 0xFFu & (voxel.PackedIDC >> 8);

    uint children = 0xFFu & voxel.PackedIDC;

    uint size = (1 << depth);

    if (children == 0u) {
      uint id = 0xFFFFu & (voxel.PackedIDC >> 16);

      payload.IsValid   = true;
      payload.TMin      = entry.TMin;
      payload.NodeMin   = entry.Min;
      payload.NodeSize  = size;
      payload.NodeIndex = entry.Index;
      payload.Id        = id;
      return payload;
    }

    uint dirX = direction.x >= 0. ? 1u : 0u;
    uint dirY = direction.y >= 0. ? 1u : 0u;
    uint dirZ = direction.z >= 0. ? 1u : 0u;

    order[0] = ((0 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (0 ^ dirZ);
    order[1] = ((0 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (1 ^ dirZ);
    order[2] = ((0 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (0 ^ dirZ);
    order[3] = ((0 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (1 ^ dirZ);
    order[4] = ((1 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (0 ^ dirZ);
    order[5] = ((1 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (1 ^ dirZ);
    order[6] = ((1 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (0 ^ dirZ);
    order[7] = ((1 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (1 ^ dirZ);

#define VISIT_CHILDC(i)                                                                                          \
  {                                                                                                              \
    uint childOctant = order[i];                                                                                 \
    uint childMask   = 1u << childOctant;                                                                        \
    if ((children & childMask) != 0u) {                                                                          \
      vec3  offset    = vec3((childOctant >> 2) & 1, (childOctant >> 1) & 1, (childOctant >> 0) & 1);            \
      float childSize = float(size / 2);                                                                         \
      vec3  childMin  = entry.Min + offset * childSize;                                                          \
      vec3  childMax  = childMin + childSize;                                                                    \
      float tMin      = 0.;                                                                                      \
      if (intersect(origin, direction, childMin, childMax, tMin) && tMin <= dist) {                              \
        stack[stackPtr++] = StackEntry(tMin, voxel.ChildIndex + bitCount(children & (childMask - 1)), childMin); \
      }                                                                                                          \
    }                                                                                                            \
  }

    VISIT_CHILDC(0);
    VISIT_CHILDC(1);
    VISIT_CHILDC(2);
    VISIT_CHILDC(3);
    VISIT_CHILDC(4);
    VISIT_CHILDC(5);
    VISIT_CHILDC(6);
    VISIT_CHILDC(7);
  }

  return payload;
}

Hit raymarchVoxels(uint offset, vec3 origin, vec3 direction, float dist) {
  Hit payload;
  payload.IsValid = false;
  payload.TMin    = 0.;

  StackEntry stack[MAX_STACK];

  uint stackPtr     = 0;
  stack[stackPtr++] = StackEntry(0., 0, vec3(0.));

  uint order[8];

  while (stackPtr > 0) {
    StackEntry entry = stack[--stackPtr];

    SVONode voxel = voxels.data[offset + entry.Index];

    uint depth = 0xFFu & (voxel.PackedIDC >> 8);

    uint children = 0xFFu & voxel.PackedIDC;

    uint size = (1 << depth);

    if (children == 0u) {
      uint id           = 0xFFFFu & (voxel.PackedIDC >> 16);
      payload.IsValid   = true;
      payload.TMin      = entry.TMin;
      payload.NodeMin   = entry.Min;
      payload.NodeSize  = size;
      payload.NodeIndex = offset + entry.Index;
      payload.Id        = id;
      return payload;
    }

    uint dirX = direction.x >= 0. ? 1u : 0u;
    uint dirY = direction.y >= 0. ? 1u : 0u;
    uint dirZ = direction.z >= 0. ? 1u : 0u;

    order[0] = ((0 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (0 ^ dirZ);
    order[1] = ((0 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (1 ^ dirZ);
    order[2] = ((0 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (0 ^ dirZ);
    order[3] = ((0 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (1 ^ dirZ);
    order[4] = ((1 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (0 ^ dirZ);
    order[5] = ((1 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (1 ^ dirZ);
    order[6] = ((1 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (0 ^ dirZ);
    order[7] = ((1 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (1 ^ dirZ);

#define VISIT_CHILDV(i)                                                                                          \
  {                                                                                                              \
    uint childOctant = order[i];                                                                                 \
    uint childMask   = 1u << childOctant;                                                                        \
    if ((children & childMask) != 0u) {                                                                          \
      vec3  offset    = vec3((childOctant >> 2) & 1, (childOctant >> 1) & 1, (childOctant >> 0) & 1);            \
      float childSize = float(size / 2);                                                                         \
      vec3  childMin  = entry.Min + offset * childSize;                                                          \
      vec3  childMax  = childMin + childSize;                                                                    \
      float tMin      = 0.;                                                                                      \
      if (intersect(origin, direction, childMin, childMax, tMin) && tMin <= dist) {                              \
        stack[stackPtr++] = StackEntry(tMin, voxel.ChildIndex + bitCount(children & (childMask - 1)), childMin); \
      }                                                                                                          \
    }                                                                                                            \
  }

    VISIT_CHILDV(0);
    VISIT_CHILDV(1);
    VISIT_CHILDV(2);
    VISIT_CHILDV(3);
    VISIT_CHILDV(4);
    VISIT_CHILDV(5);
    VISIT_CHILDV(6);
    VISIT_CHILDV(7);
  }

  StackEntry entry  = stack[stackPtr];
  payload.TMin      = entry.TMin;
  payload.NodeMin   = entry.Min;
  payload.NodeIndex = offset + entry.Index;

  return payload;
}

Hit raymarchWorld(vec3 origin, vec3 direction, float dist) {
  StackEntry stack[MAX_STACK];

  uint stackPtr     = 0;
  stack[stackPtr++] = StackEntry(0., 0, vec3(0.));

  uint order[8];

  while (stackPtr > 0) {
    StackEntry entry = stack[--stackPtr];

    SVONode voxel = chunks.data[entry.Index];

    uint depth = 0xFFu & (voxel.PackedIDC >> 8);

    uint children = 0xFFu & voxel.PackedIDC;

    uint size = (1 << depth);

    if (children == 0u) {
      uint  id            = 0xFFFFu & (voxel.PackedIDC >> 16);
      uint  offset        = headers.data[id - 1].Offset;
      float localDistance = (dist - entry.TMin) * SIZE;
      vec3  localOrigin   = (origin + direction * entry.TMin - entry.Min) * SIZE;

      Hit result = raymarchVoxels(offset, localOrigin, direction, localDistance);

      if (result.IsValid) {
        result.TMin = (entry.TMin * SIZE) + result.TMin;
        return result;
      }
    }

    uint dirX = direction.x >= 0. ? 1u : 0u;
    uint dirY = direction.y >= 0. ? 1u : 0u;
    uint dirZ = direction.z >= 0. ? 1u : 0u;

    order[0] = ((0 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (0 ^ dirZ);
    order[1] = ((0 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (1 ^ dirZ);
    order[2] = ((0 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (0 ^ dirZ);
    order[3] = ((0 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (1 ^ dirZ);
    order[4] = ((1 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (0 ^ dirZ);
    order[5] = ((1 ^ dirX) << 2) | ((0 ^ dirY) << 1) | (1 ^ dirZ);
    order[6] = ((1 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (0 ^ dirZ);
    order[7] = ((1 ^ dirX) << 2) | ((1 ^ dirY) << 1) | (1 ^ dirZ);

#define VISIT_CHILDW(i)                                                                                          \
  {                                                                                                              \
    uint childOctant = order[i];                                                                                 \
    uint childMask   = 1u << childOctant;                                                                        \
    if ((children & childMask) != 0u) {                                                                          \
      vec3  offset    = vec3((childOctant >> 2) & 1, (childOctant >> 1) & 1, (childOctant >> 0) & 1);            \
      float childSize = float(size / 2);                                                                         \
      vec3  childMin  = entry.Min + offset * childSize;                                                          \
      vec3  childMax  = childMin + childSize;                                                                    \
      float tMin      = 0.;                                                                                      \
      if (intersect(origin, direction, childMin, childMax, tMin) && tMin <= dist) {                              \
        stack[stackPtr++] = StackEntry(tMin, voxel.ChildIndex + bitCount(children & (childMask - 1)), childMin); \
      }                                                                                                          \
    }                                                                                                            \
  }

    VISIT_CHILDW(0);
    VISIT_CHILDW(1);
    VISIT_CHILDW(2);
    VISIT_CHILDW(3);
    VISIT_CHILDW(4);
    VISIT_CHILDW(5);
    VISIT_CHILDW(6);
    VISIT_CHILDW(7);
  }

  Hit payload;
  payload.IsValid = false;
  payload.TMin    = 0.;
  return payload;
}

Hit raymarch(vec3 origin, vec3 direction, float dist) {
  return raymarchWorld(origin / SIZE, direction, dist / SIZE);
}