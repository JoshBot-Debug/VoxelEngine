#version 460

#include "common.glsl"

layout(push_constant) uniform PushConstants {
  vec4 Position;
  vec4 Normal;
  uint Id;
  uint Size;
  uint Padding[2];
}
pc;

layout(location = 0) out vec4 vColor;

#include "camera.glsl"

const vec3 vertices[36] = vec3[](
    // Front (+Z)
    vec3(0, 0, 1),
    vec3(1, 0, 1),
    vec3(1, 1, 1),
    vec3(0, 0, 1),
    vec3(1, 1, 1),
    vec3(0, 1, 1),

    // Back (-Z)
    vec3(1, 0, 0),
    vec3(0, 0, 0),
    vec3(0, 1, 0),
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(1, 1, 0),

    // Left (-X)
    vec3(0, 0, 0),
    vec3(0, 0, 1),
    vec3(0, 1, 1),
    vec3(0, 0, 0),
    vec3(0, 1, 1),
    vec3(0, 1, 0),

    // Right (+X)
    vec3(1, 0, 1),
    vec3(1, 0, 0),
    vec3(1, 1, 0),
    vec3(1, 0, 1),
    vec3(1, 1, 0),
    vec3(1, 1, 1),

    // Top (+Y)
    vec3(0, 1, 1),
    vec3(1, 1, 1),
    vec3(1, 1, 0),
    vec3(0, 1, 1),
    vec3(1, 1, 0),
    vec3(0, 1, 0),

    // Bottom (-Y)
    vec3(0, 0, 0),
    vec3(1, 0, 0),
    vec3(1, 0, 1),
    vec3(0, 0, 0),
    vec3(1, 0, 1),
    vec3(0, 0, 1));

void main() {
  vec3 localPosition = vertices[gl_VertexIndex] * 1.1;

  vec3 worldPosition = pc.Position.xyz + localPosition * float(pc.Size) - 0.05;

  gl_Position = camera.viewProjection * vec4(worldPosition, 1.0);

  vColor = vec4(1.0, 0.8, 0.1, 0.3);
}