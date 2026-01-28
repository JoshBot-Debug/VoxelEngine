#version 460

layout(location=0)in vec3 vColor;
layout(location=3)out vec4 outDebug;

void main()
{
  outDebug=vec4(vColor,1.);
}
