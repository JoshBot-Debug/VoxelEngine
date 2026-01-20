const uint IA_TEX_SIZE=32;
const uint DA_TEX_SIZE=16;

struct NearestProbe{
  uint index;
  vec3 position;
  float distance;
  float spatialWeight;
};

vec2 octEncode(vec3 n){
  // The hemisphere problem
  // 0,0,-1 & 0,0,1 both return 0.5,0.5
  // But 0.00001,0.00001,+-1 returns 0.4999,0.4999 & 0.999,0.9999
  // So make sure normals don't have an exact zero, same fix intersectionAABB was done.
  // This makes this not 100% accurate but it's close enough because octUVToPixel floors it's result.
  n=mix(n,vec3(1e-6),step(abs(n),vec3(1e-8)));
  n/=(abs(n.x)+abs(n.y)+abs(n.z));
  vec2 uv=n.xy;
  if(n.z<0.)uv=(1.-abs(uv.yx))*sign(uv.xy);
  return uv*.5+.5;
}

vec3 octDecode(vec2 e){
  vec2 f=e*2.-1.;
  vec3 n=vec3(f.x,f.y,1.-abs(f.x)-abs(f.y));
  if(n.z<0.)n.xy=(1.-abs(n.yx))*sign(n.xy);
  return normalize(n);
}

vec3 normalFromIndex(uint idx,uint size){
  idx%=(size*size);
  uint x=idx%size;
  uint y=idx/size;
  vec2 uv=(vec2(x,y)+.5)/float(size);
  return octDecode(uv);
}

ivec2 indexToTile(uint index,uint tilesPerRow){
  return ivec2(index%tilesPerRow,index/tilesPerRow);
}

ivec2 octUVToPixel(vec2 octUV,ivec2 tile,uint texSize){
  ivec2 localPixel=ivec2(clamp(floor(octUV*float(texSize)),0.,float(texSize-1)));
  return tile*int(texSize)+localPixel;
}

// NearestProbe[8]findClosestProbes(vec3 position,float radius)
// {
  //   NearestProbe[8]closest;
  
  //   // Initialize
  //   for(int i=0;i<8;i++){
    //     closest[i].found=false;
    //     closest[i].position=vec3(0.);
    //     closest[i].distance=1e9;
    //     closest[i].spacialWeight=0.;
  //   }
  
  //   for(int i=0;i<probes.count;i++){
    //     vec3 probePos=probes.data[i].position;
    //     vec3 dir=probePos-position;
    //     float dist=length(dir);
    
    //     if(dist>radius)continue;
    
    //     int farthestIndex=0;
    //     float farthestDist=closest[0].distance;
    //     for(int j=1;j<8;j++){
      //       if(closest[j].distance>farthestDist){
        //         farthestDist=closest[j].distance;
        //         farthestIndex=j;
      //       }
    //     }
    
    //     if(dist<farthestDist){
      //       closest[farthestIndex].found=true;
      //       closest[farthestIndex].index=i;
      //       closest[farthestIndex].position=probePos;
      //       closest[farthestIndex].distance=dist;
    //     }
  //   }
  
  //   return closest;
// }

NearestProbe[8]findClosestProbes(vec3 position)
{
  NearestProbe result[8];
  
  // World → grid space
  vec3 local=(position-vec3(gridMin.xyz))/probeSpacing;
  ivec3 cell=ivec3(floor(local));
  
  // Clamp to valid cell range
  cell=clamp(cell,ivec3(0),gridResolution.xyz-ivec3(2));
  
  // Fraction inside the cell
  vec3 t=local-vec3(cell);
  t=clamp(t,0.,1.);
  
  int idx=0;
  for(int z=0;z<=1;z++)
  for(int y=0;y<=1;y++)
  for(int x=0;x<=1;x++){
    
    ivec3 probeCoord=cell+ivec3(x,y,z);
    
    uint probeIndex=positionToIndex(probeCoord,gridResolution.xyz);
    
    float wx=(x==0)?(1.-t.x):t.x;
    float wy=(y==0)?(1.-t.y):t.y;
    float wz=(z==0)?(1.-t.z):t.z;
    
    result[idx].index=probeIndex;
    result[idx].position=gridMin.xyz+vec3(probeCoord)*probeSpacing;
    result[idx].distance=length(result[idx].position-position);
    result[idx].spatialWeight=wx*wy*wz;
    
    idx++;
  }
  
  return result;
}

vec4 loadProbeIrradiance(ivec2 pixel){
  #ifdef SAMPLE_PROBE_IMAGE_2D
  return imageLoad(irradianceAtlas,pixel);
  #else
  vec2 uv=vec2(pixel)/vec2(iaWidth,iaHeight);
  return texture(irradianceAtlas,uv);
  #endif
}

vec4 loadProbeGaussianDepth(ivec2 pixel){
  #ifdef SAMPLE_PROBE_IMAGE_2D
  return imageLoad(gaussianDepthAtlas,pixel);
  #else
  vec2 uv=vec2(pixel)/vec2(daWidth,daHeight);
  return texture(gaussianDepthAtlas,uv);
  #endif
}

vec3 sampleProbes(vec3 position,vec3 normal)
{
  NearestProbe probes[8]=findClosestProbes(position);
  
  vec3 result=vec3(0.);
  float totalWeight=0.;
  
  vec2 octUV=octEncode(normal);
  
  for(int i=0;i<8;i++){
    NearestProbe p=probes[i];
    
    // --- Probe textures ---
    ivec2 iaTile=indexToTile(p.index,iaTilesPerRow);
    ivec2 iaPixel=octUVToPixel(octUV,iaTile,IA_TEX_SIZE);
    
    ivec2 daTile=indexToTile(p.index,daTilesPerRow);
    ivec2 daPixel=octUVToPixel(octUV,daTile,DA_TEX_SIZE);
    
    vec3 irradiance=loadProbeIrradiance(iaPixel).rgb;
    vec2 depthMoments=loadProbeGaussianDepth(daPixel).rg;
    
    // --- Visibility ---
    // float mean=depthMoments.x;
    // float variance=max(depthMoments.y-mean*mean,1e-6);
    // float sigma=sqrt(variance);
    
    // float delta=p.distance-mean;
    // float visibility=exp(-.5*(delta*delta)/variance);
    
    float mean=depthMoments.x;
    float variance=max(depthMoments.y-mean*mean,1e-6);
    
    float d=p.distance;
    
    // If point is in front of the mean occluder → fully visible
    float visibility;
    if(d<=mean){
      visibility=1.;
    }else{
      float delta=d-mean;
      visibility=variance/(variance+delta*delta);
    }
    
    // --- Spatial weight (trilinear) ---
    float spatialWeight=p.spatialWeight;
    
    float w=visibility*spatialWeight;
    
    result+=irradiance*w;
    totalWeight+=w;
  }
  
  return totalWeight>0.?result/totalWeight:vec3(0.);
}

// vec3 sampleProbes(vec3 position,vec3 normal){
  //   NearestProbe[8]closestProbes=findClosestProbes(position);
  
  //   float weight=0.;
  //   vec3 light=vec3(0.);
  
  //   vec2 octUV=octEncode(normal);
  
  //   for(int i=0;i<8;i++){
    //     NearestProbe probe=closestProbes[i];
    
    //     ivec2 iaTile=indexToTile(probe.index,iaTilesPerRow);
    //     ivec2 iaPixel=octUVToPixel(octUV,iaTile,IA_TEX_SIZE);
    
    //     ivec2 daTile=indexToTile(probe.index,daTilesPerRow);
    //     ivec2 daPixel=octUVToPixel(octUV,daTile,DA_TEX_SIZE);
    
    //     vec3 color=loadProbeIrradiance(iaPixel).rgb;
    //     vec2 depthMoments=loadProbeGaussianDepth(daPixel).rg;
    
    //     // Distance to the probe
    //     float distance=max(probe.distance,.01)/worldSize;
    
    //     float mean=depthMoments.r*worldSize;
    //     float m2=depthMoments.g*(worldSize*worldSize);
    //     float variance=max(m2-mean*mean,0.);
    
    //     // Gaussian PDF-style weight
    //     float sigma=sqrt(max(variance,1e-6));
    //     float delta=probe.distance-mean;
    
    //     // Gaussian falloff (higher if d ≈ mean, lower if d is far away)
    //     float visibility=exp(-.5*(delta/sigma)*(delta/sigma));
    
    //     // NdotL the intensity of the incoming radiance
    //     float intensity=max(dot(normal,normalize(probe.position-position)),0.);
    
    //     // Distance falloff weight
    //     float attenuation=1./(distance*distance);
    
    //     // The weight
    //     float w=visibility*intensity*attenuation;
    //     // float w=1.;
    
    //     light+=color*w;
    //     weight+=w;
  //   }
  
  //   if(weight>0.)
  //   light/=weight;
  
  //   return light;
// }

vec4 computeLightInfluence(vec3 hitPosition,vec3 normal,vec3 lightCenter,vec3 halfExtents,uint probeIndex,uint rayIndex){
  const int samples=8;// tweak: 1..8 typical. Use temporal accumulation to increase effective samples.
  vec4 color=vec4(0.);
  float hitCount=0.;
  float occlusionWeight=0.;
  
  // Setup RNG per-thread
  uint seed=uint(probeIndex)*1664525u+uint(rayIndex)*1013904223u+0x9E3779B9u;
  
  // offset the origin to avoid self-hit
  float eps=.0001;
  vec3 origin=hitPosition+normal*eps;
  
  for(int s=0;s<samples;++s){
    // stratified + jittered sampling over unit square
    float u=(float(s)+rndFloat(seed))/float(samples);// simple stratification on one axis
    float v=rndFloat(seed);
    
    // sample point on light box surface or area - here we sample *on the box surface interior*
    // map u,v to a point inside the box: uniform inside volume or on surface; for soft shadow sampling the surface is fine
    // We'll sample uniformly on the rectangle centered at lightCenter with halfExtents
    vec3 offset=vec3(
      mix(-halfExtents.x,halfExtents.x,u),
      mix(-halfExtents.y,halfExtents.y,v),
      mix(-halfExtents.z,halfExtents.z,rndFloat(seed))
    );
    
    vec3 samplePos=lightCenter+offset;
    vec3 L=samplePos-origin;
    float distToSample=length(L);
    vec3 Ldir=L/distToSample;
    
    Hit hit=raymarch(origin,Ldir,distToSample);
    
    if(hit.IsValid)
    {
      Material material=getMaterial(hit.Material);
      color.a+=1.-material.Emissive.a;
      color.rgb+=material.Emissive.rgb;
      occlusionWeight+=color.a;
      hitCount+=1.;
    }
  }
  
  if(occlusionWeight>0.)
  color.a=1.-(color.a/occlusionWeight);
  
  if(hitCount>0.)
  color.rgb=(color.rgb/hitCount);
  
  return color;
}