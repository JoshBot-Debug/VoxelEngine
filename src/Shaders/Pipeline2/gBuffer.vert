#version 460

#include "common.glsl"

layout(location=0)in vec3 inPosition;
layout(location=1)in vec3 inNormal;
layout(location=2)in uint inMaterial;

layout(location=0)out vec3 vNormal;
layout(location=1)out uint vMaterial;
layout(location=2)out vec2 vMotionVector;
layout(location=3)out vec3 vWorldPosition;
layout(location=4)out vec3 vWorldNormal;

#include "camera.glsl"

void main(){
  vec4 worldPosition=vec4(inPosition,1.);
  vec4 currentClip=camera.viewProjection*worldPosition;
  vec2 currentNDC=currentClip.xy/currentClip.w;
  vec4 previousClip=camera.pViewProjection*worldPosition;
  vec2 previousNDC=previousClip.xy/previousClip.w;
  
  vWorldPosition = worldPosition.xyz;
  vWorldNormal = normalize(inNormal);

  vNormal=normalize((mat3(camera.view)*inNormal))*.5+.5;
  vMaterial=inMaterial;
  vMotionVector=currentNDC-previousNDC;

  gl_Position=currentClip;
}