const float FMAX=1e9;
const float PI=3.1415926535897932;

struct Vertex{
  uint Id;
  vec3 Position;
  vec3 Normal;
  uint PadN;
};

struct DenseVoxel{
  uint Material;
  uint Depth;
  vec3 Position;
};

struct Hit{
  bool IsValid;
  float TMin;
  vec3 NodeMin;
  uint NodeSize;
  uint NodeIndex;
  uint Material;
};

struct Payload{
  vec4 albedo;
  vec4 direct;
  vec4 indirect;
  vec3 normal;
  vec3 position;
  float depth;
  float occlusion;
  float metallic;
  float roughness;
  float emissive;
};

struct DDGIProbe{
  vec3 position;
};

struct Ray{
  vec3 origin;
  vec3 direction;
};

bool intersectAABB(vec3 ro,vec3 rd,vec3 boxMin,vec3 boxMax,out float tmin){
  // To avoid division by zero, make sure x/y/z is not zero
  rd=mix(rd,vec3(1e-6),step(abs(rd),vec3(1e-8)));
  vec3 invDir=1./rd;
  vec3 t0=(boxMin-ro)*invDir;
  vec3 t1=(boxMax-ro)*invDir;
  
  vec3 tminVec=min(t0,t1);
  vec3 tmaxVec=max(t0,t1);
  
  tmin=max(max(tminVec.x,tminVec.y),tminVec.z);
  float tmax=min(min(tmaxVec.x,tmaxVec.y),tmaxVec.z);
  
  return tmax>=max(tmin,0.);
}

vec3 getVoxelNormal(vec3 rayOrigin,vec3 rayDirection,Hit payload){
  
  vec3 boxMin=payload.NodeMin;
  vec3 boxMax=payload.NodeMin+payload.NodeSize;
  
  vec3 invDir=1./rayDirection;
  vec3 t0=(boxMin-rayOrigin)*invDir;
  vec3 t1=(boxMax-rayOrigin)*invDir;
  
  vec3 tminVec=min(t0,t1);
  float tmax=max(max(tminVec.x,tminVec.y),tminVec.z);
  
  if(tmax==tminVec.x)return vec3(-sign(rayDirection.x),0.,0.);
  if(tmax==tminVec.y)return vec3(0.,-sign(rayDirection.y),0.);
  return vec3(0.,0.,-sign(rayDirection.z));
}

bool raySphereIntersect(vec3 rayOrigin,vec3 rayDir,vec3 sphereCenter,float sphereRadius,out float tHit)
{
  vec3 oc=rayOrigin-sphereCenter;
  float b=dot(rayDir,oc);
  float c=dot(oc,oc)-sphereRadius*sphereRadius;
  float discriminant=b*b-c;
  
  if(discriminant<0.){
    return false;// no intersection
  }
  
  // Closest intersection
  tHit=-b-sqrt(discriminant);
  if(tHit<0.){
    // Ray starts inside sphere or behind sphere, take far intersection
    tHit=-b+sqrt(discriminant);
  }
  
  if(tHit<0.){
    return false;// sphere is completely behind ray
  }
  
  return true;
}

vec3 sampleHemisphere(uint index,uint samples)
{
  float u=(float(index)+.5)/float(samples);// [0,1]
  float v=fract(float(index)*.6180339887498948);// golden ratio fractional step
  
  float theta=2.*PI*v;// azimuth
  float z=1.-2.*u;// [-1,1]
  float r=sqrt(max(0.,1.-z*z));
  
  return vec3(r*cos(theta),r*sin(theta),z);
}

vec3 sampleHemisphereAO(float r1,float r2)
{
  float r=sqrt(r1);
  float theta=2.*PI*r2;// azimuth
  
  float x=r*cos(theta);
  float y=r*sin(theta);
  float z=sqrt(max(0.,1.-r1));// more weight near normal
  
  // This is in tangent space (z = normal direction)
  return vec3(x,y,z);
}

vec3 sampleCubeFace(uint index)
{
  // Make sure index is in [0,5]
  uint face=index%6u;
  
  if(face==0u)return vec3(1.,0.,0.);// +X (right)
  if(face==1u)return vec3(-1.,0.,0.);// -X (left)
  if(face==2u)return vec3(0.,1.,0.);// +Y (top)
  if(face==3u)return vec3(0.,-1.,0.);// -Y (bottom)
  if(face==4u)return vec3(0.,0.,1.);// +Z (front)
  return vec3(0.,0.,-1.);// -Z (back)
}

vec3 sampleCosineHemisphere(vec3 normal,uint index,uint samples){
  float u=(float(index)+.5)/float(samples);// [0,1]
  float v=fract(float(index)*.6180339887498948);// golden ratio fractional step
  
  // Map (u,v) to polar coordinates for cosine-weighted sampling
  float r=sqrt(u);
  float theta=2.*3.14159265359*v;
  
  // Local tangent space coordinates
  float x=r*cos(theta);
  float y=r*sin(theta);
  float z=sqrt(max(0.,1.-u));// ensure z >= 0
  
  // Build orthonormal basis from the surface normal
  vec3 tangent=normalize(abs(normal.z)<.999?cross(normal,vec3(0,0,1))
  :cross(normal,vec3(0,1,0)));
  vec3 bitangent=cross(normal,tangent);
  
  // Transform to world space
  return x*tangent+y*bitangent+z*normal;
}

vec3 sampleCosineHemisphereRandom(vec3 normal,float r1,float r2){
  // Map (u,v) to polar coordinates for cosine-weighted sampling
  float r=sqrt(r1);
  float theta=2.*3.14159265359*r2;
  
  // Local tangent space coordinates
  float x=r*cos(theta);
  float y=r*sin(theta);
  float z=sqrt(max(0.,1.-r1));
  
  // Build orthonormal basis from the surface normal
  vec3 tangent=normalize(abs(normal.z)<.999?cross(normal,vec3(0,0,1))
  :cross(normal,vec3(0,1,0)));
  vec3 bitangent=cross(normal,tangent);
  
  // Transform to world space
  return x*tangent+y*bitangent+z*normal;
}

uint wangHash(uint s){
  s=(s^61u)^(s>>16u);
  s*=9u;
  s=s^(s>>4u);
  s*=0x27d4eb2du;
  s=s^(s>>15u);
  return s;
}

float rndFloat(inout uint seed){
  seed=wangHash(seed);
  return float(seed)*(1./4294967296.);
}

float random(uvec2 pixel,uint a,uint b){
  uint seed=pixel.x*1973u^pixel.y*9277u^a*26699u^b*8887u;
  return float(wangHash(seed))/4294967296.;// divide by 2^32
}

// ----- PBR helpers -----

// Schlick Fresnel approximation
vec3 fresnelSchlick(float cosTheta,vec3 F0){
  return F0+(1.-F0)*pow(1.-cosTheta,5.);
}

// GGX / Trowbridge-Reitz normal distribution function
float DistributionGGX(vec3 N,vec3 H,float roughness){
  float a=roughness*roughness;
  float a2=a*a;
  float NdotH=max(dot(N,H),0.);
  float NdotH2=NdotH*NdotH;
  float denom=(NdotH2*(a2-1.)+1.);
  denom=PI*denom*denom;
  return a2/max(denom,1e-6);
}

// Geometry Schlick-GGX
float GeometrySchlickGGX(float NdotV,float roughness){
  float r=roughness+1.;
  float k=(r*r)/8.;
  return NdotV/(NdotV*(1.-k)+k);
}

float GeometrySmith(vec3 N,vec3 V,vec3 L,float roughness){
  float NdotV=max(dot(N,V),0.);
  float NdotL=max(dot(N,L),0.);
  float ggx1=GeometrySchlickGGX(NdotV,roughness);
  float ggx2=GeometrySchlickGGX(NdotL,roughness);
  return ggx1*ggx2;
}

// Build tangent/bitangent given normal (stable)
void buildTangentSpace(vec3 N,out vec3 T,out vec3 B){
  vec3 up=abs(N.z)<.999?vec3(0.,0.,1.):vec3(1.,0.,0.);
  T=normalize(cross(up,N));
  B=cross(N,T);
}

// Sample GGX half-vector (world-space) given N, roughness, two randoms in [0,1]
vec3 sampleGGX(vec3 N,float roughness,float u1,float u2){
  // Sample in tangent space
  float a=roughness*roughness;
  float phi=2.*PI*u1;
  float cosTheta=sqrt((1.-u2)/(1.+(a*a-1.)*u2));// invert CDF for GGX
  float sinTheta=sqrt(max(0.,1.-cosTheta*cosTheta));
  vec3 H_t=vec3(cos(phi)*sinTheta,sin(phi)*sinTheta,cosTheta);
  
  // Transform H_t to world space
  vec3 T,B;
  buildTangentSpace(N,T,B);
  vec3 H=normalize(T*H_t.x+B*H_t.y+N*H_t.z);
  return H;
}

// Evaluate Cook-Torrance BRDF (returns RGB specular+diffuse)
// Inputs:
//   N, V (view dir toward camera), L (light dir toward light), material params
// Also returns the pdf for the sampled direction if needed (computed externally)
vec3 evaluatePBR(vec3 N,vec3 V,vec3 L,vec3 albedo,float metallic,float roughness){
  float NdotL=max(dot(N,L),0.);
  float NdotV=max(dot(N,V),0.);
  if(NdotL<=0.||NdotV<=0.)return vec3(0.);
  
  // Fresnel at normal incidence
  vec3 F0=mix(vec3(.04),albedo,metallic);
  vec3 H=normalize(V+L);
  float NdotH=max(dot(N,H),0.);
  float VdotH=max(dot(V,H),0.);
  
  float D=DistributionGGX(N,H,roughness);
  float G=GeometrySmith(N,V,L,roughness);
  vec3 F=fresnelSchlick(VdotH,F0);
  
  vec3 specNumerator=D*G*F;
  float denom=4.*NdotV*NdotL+1e-6;
  vec3 specular=specNumerator/denom;
  
  // kD is (1 - F) * (1 - metallic)
  vec3 kD=(vec3(1.)-F)*(1.-metallic);
  
  vec3 diffuse=(albedo/PI)*kD;
  
  return diffuse+specular;
}

vec3 BRDF(vec3 N,vec3 V,vec3 L,vec3 albedo,float metallic,float roughness){
  vec3 H=normalize(V+L);
  
  float NdotL=max(dot(N,L),0.);
  float NdotV=max(dot(N,V),0.);
  float NdotH=max(dot(N,H),0.);
  float VdotH=max(dot(V,H),0.);
  
  // Fresnel
  vec3 F0=mix(vec3(.04),albedo,metallic);
  vec3 F=fresnelSchlick(VdotH,F0);
  
  // Microfacet
  float D=DistributionGGX(N,H,roughness);
  float G=GeometrySmith(N,H,L,roughness);
  
  vec3 specular=(F*D*G)/max(4.*NdotV*NdotL,1e-4);
  
  // Lambertian diffuse (energy-conserving, only if not metallic)
  vec3 kd=(1.-F)*(1.-metallic);
  vec3 diffuse=kd*albedo/PI;
  
  return(diffuse+specular)*NdotL;
}

// Compute pdf of sampling L when we sampled according to mixture of GGX half-vector sampling + cosine
// If sampledSpecular==true, the chosen l was produced by GGX half-vector sampling (converted to L)
// pdf_spec = D(N,H) * NdotH / (4 * VdotH)
// pdf_diff = NdotL / PI
float pdfGGXtoL(vec3 N,vec3 V,vec3 H,float roughness){
  float D=DistributionGGX(N,H,roughness);
  float NdotH=max(dot(N,H),0.);
  float VdotH=max(dot(V,H),1e-6);
  // pdf of H when sampling GGX (in H-space) is: p_H = D * NdotH
  // conversion to pdf_L: p_L = p_H / (4 * VdotH)
  float pH=D*NdotH;
  return pH/(4.*VdotH+1e-9);
}

// ----- End PBR helpers -----

// ----- Tone mapping --------
vec3 ACESFilmicToneMapping(vec3 color)
{
  // sRGB gamut -> ACES approximation
  const float a=2.51;
  const float b=.03;
  const float c=2.43;
  const float d=.59;
  const float e=.14;
  
  return(color*(a*color+b))/(color*(c*color+d)+e);
}

ivec3 indexToPosition(uint index,ivec3 grid)
{
  int x=int(index%uint(grid.x));
  int y=int((index/uint(grid.x))%uint(grid.y));
  int z=int(index/uint(grid.x*grid.y));
  
  return ivec3(x,y,z);
}

int positionToIndex(ivec3 position,ivec3 grid)
{
  return position.x+position.y*grid.x+position.z*grid.x*grid.y;
}