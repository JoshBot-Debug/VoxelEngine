#version 450
layout(location=0)out vec4 outNormal;
layout(location=1)out uint outMaterial;
layout(location=2)out vec2 outMotionVector;

layout(location=0)in vec3 vNormal;
layout(location=1)in flat uint vMaterial;
layout(location=2)in vec2 vMotionVector;

void main(){
  outNormal=vec4(vNormal,1.);
  outMaterial=vMaterial;
  outMotionVector=vMotionVector;
}