#version 450

#include "common.glsl"

layout(set=0,binding=0)uniform Metadata{
  uint frame;
  uint worldSize;
};

layout(location=0)in vec3 inPosition;
layout(location=1)in vec3 inNormal;
layout(location=2)in uint inMaterial;

layout(location=0)out vec3 vNormal;
layout(location=1)out uint vMaterial;
layout(location=2)out vec2 vMotionVector;

#include "camera.glsl"

void main(){
  vec4 worldPos=vec4(inPosition,1.);
  vec4 currentClip=camera.viewProjection*worldPos;
  vec2 currentNDC=currentClip.xy/currentClip.w;
  vec4 previousClip=camera.pViewProjection*worldPos;
  vec2 previousNDC=previousClip.xy/previousClip.w;
  
  vNormal=normalize((mat3(camera.view)*inNormal))*.5+.5;
  vMaterial=inMaterial;
  vMotionVector=currentNDC-previousNDC;

  gl_Position=currentClip;
}