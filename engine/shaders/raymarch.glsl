#include "raymarchChunks.glsl"

struct FlatNodeHeader {
  uint Id;
  uint Offset;
  uint Size;
};

layout(std430, set = 1, binding = 50) readonly buffer VoxelOctrees {
  FlatNode data[];
}
voxels;

layout(std430, set = 1, binding = 58) readonly buffer VoxelOctreeHeaders {
  uint           count;
  uint           padding[3];
  FlatNodeHeader data[];
}
headers;

struct RaymarchStackEntry {
  uint  ChunkId;
  float TMin;
  vec3  Origin;
};

struct DebugRay {
  vec4 Start;
  vec4 End;
  vec4 Color;
};

layout(std430, set = 1, binding = 59) buffer DebugRayBuffer {
  DebugRay rays[];
}
debugRayBuffer;

void debugRay(ivec3 voxel, vec3 rayStart, vec3 rayEnd, vec3 color, uint index) {
  ivec3 originVoxel = ivec3(floor(rayStart));
  if (any(notEqual(originVoxel, voxel)))
    return;
  if (debugRayBuffer.rays[index].Color.a != 0.0)
    return;
  debugRayBuffer.rays[index].Start = vec4(rayStart, 1.0);
  debugRayBuffer.rays[index].End   = vec4(rayEnd, 1.0);
  debugRayBuffer.rays[index].Color = vec4(color, 1.0);
}

const uint RAYMARCH_MAX_STACK = 64;

uint getChunkId(vec3 origin) {
  // TODO need to pass in 64 as the size of the voxel chunk
  uint CHUNK_SIZE = 64;
  uint x          = uint(origin.x) / CHUNK_SIZE;
  uint y          = uint(origin.y) / CHUNK_SIZE;
  uint z          = uint(origin.z) / CHUNK_SIZE;

  uint i = x + (y * 64 + (z * 64 * 64));

  return i;
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

    /// TODO: Need to find a way to index voxel svos here
    FlatNode voxel = voxels.data[offset + entry.Index];

    uint depth = 0xFFu & (voxel.PackedIDC >> 8);

    uint children = 0xFFu & voxel.PackedIDC;

    uint size = (1 << depth);

    if (children == 0u) {
      uint id = 0xFFFFu & (voxel.PackedIDC >> 16);

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

#define VISIT_CHILD(i)                                                                                           \
  {                                                                                                              \
    uint childOctant = order[i];                                                                                 \
    uint childMask   = 1u << childOctant;                                                                        \
    if ((children & childMask) != 0u) {                                                                          \
      vec3  offset    = vec3((childOctant >> 2) & 1, (childOctant >> 1) & 1, (childOctant >> 0) & 1);            \
      float childSize = float(size / 2);                                                                         \
      vec3  childMin  = entry.Min + offset * childSize;                                                          \
      vec3  childMax  = childMin + childSize;                                                                    \
      float tMin      = 0.;                                                                                      \
      if (intersectAABB(origin, direction, childMin, childMax, tMin) && tMin <= dist) {                          \
        stack[stackPtr++] = StackEntry(tMin, voxel.ChildIndex + bitCount(children & (childMask - 1)), childMin); \
      }                                                                                                          \
    }                                                                                                            \
  }

    VISIT_CHILD(0);
    VISIT_CHILD(1);
    VISIT_CHILD(2);
    VISIT_CHILD(3);
    VISIT_CHILD(4);
    VISIT_CHILD(5);
    VISIT_CHILD(6);
    VISIT_CHILD(7);
  }

  StackEntry entry  = stack[stackPtr];
  payload.TMin      = entry.TMin;
  payload.NodeMin   = entry.Min;
  payload.NodeIndex = offset + entry.Index;

  return payload;
}

float tNextBoundary(vec3 origin, vec3 direction) {
  vec3 stepDir = sign(direction);

  vec3 nextBoundary = mix(vec3(0.), vec3(64.), greaterThan(direction, vec3(0.)));

  vec3 tVals = (nextBoundary - origin) / direction;

  tVals = mix(vec3(1e30), tVals, notEqual(direction, vec3(0.)));

  return min(tVals.x, min(tVals.y, tVals.z));
}

Hit raymarch(vec3 origin, vec3 direction, float dist) {
  Hit result;
  result.IsValid = false;
  result.TMin    = dist;

  float t   = 0.0;
  float EPS = .0001;

  int MAX_CHUNK_ITERATIONS = 8;

  while (t < dist) {
    MAX_CHUNK_ITERATIONS--;

    if (MAX_CHUNK_ITERATIONS == 0)
      break;

    vec3 P = origin + direction * t;

    // Find next chunk along ray
    Hit chunk = raymarchChunks(P / 64.0, direction, dist - t);

    if (!chunk.IsValid)
      break;

    // Chunk hit distance relative to current origin
    float chunkT = chunk.TMin;

    // Ray position inside chunk
    vec3 chunkOrigin = mod(P + direction * (chunkT * 64.), 64.0);

    // Distance available inside this chunk
    float chunkDist = min(tNextBoundary(chunkOrigin, direction), dist - t - chunkT);

    // Traverse voxels within chunk
    result = raymarchVoxels(headers.data[chunk.Id - 1].Offset, chunkOrigin, direction, chunkDist);

    if (result.IsValid) {
      result.TMin += t + chunkT;
      return result;
    }

    // Continue past this chunk
    t += chunkT + chunkDist + EPS;
  }

  result.IsValid = false;
  result.TMin    = dist;

  return result;
}