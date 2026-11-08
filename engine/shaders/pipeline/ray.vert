#version 460

#include "common.glsl"

struct DrawRay {
  vec4 Origin;
  vec4 Direction;
  vec4 Color;
};

layout(set = 1, binding = 59, std430) readonly buffer RayBuffer {
  uint    count;
  uint    padding[3];
  DrawRay rays[];
};

layout(location = 0) out vec4 vColor;

#include "camera.glsl"

void main() {
  DrawRay ray = rays[gl_InstanceIndex];

  vec3 direction = normalize(ray.Direction.xyz);
  vec3 origin    = ray.Origin.xyz  + direction * 0.1;

  vec3 position = origin;

  if (gl_VertexIndex == 1) {
    position += direction * ray.Direction.w;
  }

  gl_Position = camera.viewProjection * vec4(position, 1.0);

  vColor = ray.Color;
}