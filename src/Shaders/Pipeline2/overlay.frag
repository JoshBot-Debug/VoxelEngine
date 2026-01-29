#version 460

layout(location=0)in vec3 vColor;
layout(location=0)out vec4 outputImage;

void main()
{
  outputImage=vec4(vColor,1.);
}
