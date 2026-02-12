
layout(set=0,binding=0)uniform Camera{
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
  mat4 inverseView;
  mat4 inverseProjection;
  mat4 inverseViewProjection;
  mat4 pView;
  mat4 pProjection;
  mat4 pViewProjection;
  mat4 pInverseView;
  mat4 pInverseProjection;
  mat4 pInverseViewProjection;
  vec3 position;
  float farPlane;
}camera;

Ray generateCameraRay(vec2 uv){
  vec2 ndc=vec2(uv*2.-1.);
  vec4 clip=vec4(ndc,0.,1.);
  vec4 view=camera.inverseProjection*clip;
  view/=view.w;
  
  vec3 localOrigin=vec3(0.);
  vec3 localDir=normalize(vec3(view));
  
  vec3 worldOrigin=(camera.inverseView*vec4(localOrigin,1.)).xyz;
  vec3 worldDir=normalize((camera.inverseView*vec4(localDir,0.)).xyz);
  
  return Ray(worldOrigin,worldDir);
}

vec3 depthToViewSpace(vec2 uv,float depth)
{
  vec2 ndc=vec2(uv*2.-1.);
  vec4 clip=vec4(ndc,depth,1.);
  vec4 view=camera.inverseProjection*clip;
  view/=view.w;
  
  return vec3(view);
}

vec3 depthToWorldSpace(vec2 uv,float depth)
{
  vec3 view=depthToViewSpace(uv,depth);
  return vec3(camera.inverseView*vec4(view,1.));
}
