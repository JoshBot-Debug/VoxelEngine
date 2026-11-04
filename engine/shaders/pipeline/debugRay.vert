#version 460

#include "common.glsl"

struct DebugRay {
    vec4 Start;
    vec4 End;
    vec4 Color;
};

layout(std430, set = 1, binding = 59) readonly buffer DebugRayBuffer {
  DebugRay rays[];
};

layout(location = 0) out vec3 vColor;

const float THICKNESS = 3.0; // pixels

#include "camera.glsl"

void main() {
  uint rayIndex = gl_VertexIndex / 6;
  uint vertex   = gl_VertexIndex % 6;

  DebugRay ray = rays[rayIndex];

  vec4 startClip = camera.viewProjection * ray.Start;
  vec4 endClip   = camera.viewProjection * ray.End;

  vec2 startNDC = startClip.xy / startClip.w;
  vec2 endNDC   = endClip.xy / endClip.w;

  vec2 direction = normalize(endNDC - startNDC);
  vec2 normal    = vec2(-direction.y, direction.x);

  // Convert pixel thickness to NDC.
  vec2 thickness = THICKNESS / vec2(1024,1024);

  vec2 offsets[6] = vec2[](
      -normal, normal, -normal,
      -normal, normal, normal
  );

  vec4 positions[6] = vec4[](
      startClip, startClip, endClip,
      endClip, startClip, endClip
  );

  vec2 offset = offsets[vertex] * thickness * positions[vertex].w;

  gl_Position = positions[vertex];
  gl_Position.xy += offset;

  vColor = ray.Color.rgb;
}