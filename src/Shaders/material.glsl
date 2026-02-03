struct Material{
  uint Id;
  float Metallic;
  float Roughness;
  uint _Padding;
  vec4 Albedo;
  vec4 Emissive;
};

layout(std430,set=1,binding=51)readonly buffer Materials{
  uint count;
  uint padding[3];
  Material data[];
}materials;

layout(std430,set=1,binding=52)readonly buffer MaterialLUT{
  uint count;
  uint padding[3];
  uint data[];
}materialLUT;

Material getMaterial(uint id)
{
  uint i=materialLUT.data[id];
  if(i==0||i>materialLUT.count)
  return Material(
    0u,
    0.,0.,0u,
    vec4(0.),
    vec4(0.)
  );
  
  return materials.data[i-1];
}