#version 460

#include "common.glsl"

layout(location=0)in vec3 inPos;
layout(location=1)in vec3 inColor;

layout(location=0)out vec3 vColor;

#include "camera.glsl"

void main(){
  vColor=inColor;
  gl_Position=camera.viewProjection*vec4(inPos,1.);
}
