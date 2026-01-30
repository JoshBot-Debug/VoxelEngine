#version 460

#include "common.glsl"

layout(location=0)in vec3 inPosition;
layout(location=1)in vec3 inColor;

layout(location=0)out vec3 vColor;

layout(set=1,binding=101)uniform sampler2D depthTexture;

#include "camera.glsl"

void main(){
  vColor=inColor;
  vec4 worldPosition=vec4(inPosition,1.);
  vec4 currentClip=camera.viewProjection*worldPosition;
  gl_Position=currentClip;
}