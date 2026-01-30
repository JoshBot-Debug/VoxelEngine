#version 460

layout(location=0)out vec4 outNormal;
layout(location=1)out uint outMaterial;
layout(location=2)out vec2 outMotionVector;

layout(location=0)in vec3 vNormal;
layout(location=1)in flat uint vMaterial;
layout(location=2)in vec2 vMotionVector;
layout(location=3)in vec3 vWorldPosition;
layout(location=4)in vec3 vWorldNormal;

float drawGrid(vec2 local)
{
  vec2 d=min(local,1.-local);
  
  float width=.02;
  
  vec2 a=smoothstep(
    vec2(width),
    vec2(width)+fwidth(local),
    d
  );
  
  return 1.-min(a.x,a.y);
}

void main(){
  outNormal=vec4(vNormal,1.);
  outMaterial=vMaterial;
  outMotionVector=vMotionVector;
  
  /// TODO: Draw the mesh grid style. Helpful for debugging, keep this code :P, move it out later.
  // vec2 gridPos=vec2(0.);
  
  // vec3 n=normalize(vWorldNormal);
  // vec3 an=abs(n);
  
  // if(an.y>an.x&&an.y>an.z)
  // {
    //   // top or bottom
    //   gridPos=vWorldPos.xz;
  // }
  // else if(an.x>an.y&&an.x>an.z)
  // {
    //   // left or right
    //   gridPos=vWorldPos.zy;
  // }
  // else
  // {
    //   // front or back
    //   gridPos=vWorldPos.xy;
  // }
  
  // ivec2 cell=ivec2(floor(gridPos));
  // vec2 local=fract(gridPos);
  
  // float grid=drawGrid(local);
  
  // vec3 color=mix(
    //   vec3(.3412,.7608,.2353),
    //   vec3(0.),
    //   grid
  // );
  
  // outNormal=vec4(color,1.);
}