#version 460

#include "common.glsl"

struct UIRay {
  vec4 Origin;
  vec4 Direction;
};

layout(push_constant) uniform PushConstants {
  vec4 Position;
  vec4 Normal;
  uint Size;
  uint Padding[3];
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

void drawVoxel() {
  vec3 localPosition = vertices[gl_VertexIndex] * 1.1;

  vec3 worldPosition =
      pc.Position.xyz + localPosition * float(pc.Size) - 0.05;

  gl_Position = camera.viewProjection * vec4(worldPosition, 1.0);

  const vec3 faceNormals[6] = vec3[](
      vec3(0, 0, 1),
      vec3(0, 0, -1),
      vec3(-1, 0, 0),
      vec3(1, 0, 0),
      vec3(0, 1, 0),
      vec3(0, -1, 0));

  int faceIndex = gl_VertexIndex / 6;

  bool selectedFace =
      dot(faceNormals[faceIndex], normalize(pc.Normal.xyz)) > 0.99;

  vColor = selectedFace ? vec4(1.0, 0.2, 0.1, 0.6) : vec4(1.0, 0.8, 0.1, 0.2);
}

void main() {
  drawVoxel();
}