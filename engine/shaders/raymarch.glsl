#include "raymarchChunks.glsl"

struct FlatNodeHeader{
  uint Id;
  uint Offset;
  uint Size;
};

layout(std430,set=1,binding=50)readonly buffer VoxelOctrees{
  FlatNode data[];
}voxels;

layout(std430,set=1,binding=58)readonly buffer VoxelOctreeHeaders{
  uint count;
  uint padding[3];
  FlatNodeHeader data[];
}headers;

struct RaymarchStackEntry{
  uint ChunkId;
  float TMin;
  vec3 Origin;
};

const uint RAYMARCH_MAX_STACK=8;

uint getChunkId(vec3 origin)
{
  // TODO need to pass in 64 as the size of the voxel chunk
  uint CHUNK_SIZE=64;
  uint x=uint(origin.x)/CHUNK_SIZE;
  uint y=uint(origin.y)/CHUNK_SIZE;
  uint z=uint(origin.z)/CHUNK_SIZE;
  
  uint i=x+(y*64+(z*64*64));
  
  return i;
}

Hit raymarchVoxels(uint offset,vec3 origin,vec3 direction,float dist)
{
  Hit payload;
  payload.IsValid=false;
  payload.TMin=0.;
  
  StackEntry stack[MAX_STACK];
  
  uint stackPtr=0;
  stack[stackPtr++]=StackEntry(0.,0,vec3(0.));
  
  uint order[8];
  
  while(stackPtr>0){
    StackEntry entry=stack[--stackPtr];
    
    /// TODO: Need to find a way to index voxel svos here
    FlatNode voxel=voxels.data[offset+entry.Index];
    
    uint depth=0xFFu&(voxel.PackedIDC>>8);
    
    uint children=0xFFu&voxel.PackedIDC;
    
    uint size=(1<<depth);
    
    if(children==0u){
      uint id=0xFFFFu&(voxel.PackedIDC>>16);
      
      payload.IsValid=true;
      payload.TMin=entry.TMin;
      payload.NodeMin=entry.Min;
      payload.NodeSize=size;
      payload.NodeIndex=offset+entry.Index;
      payload.Id=id;
      return payload;
    }
    
    uint dirX=direction.x>=0.?1u:0u;
    uint dirY=direction.y>=0.?1u:0u;
    uint dirZ=direction.z>=0.?1u:0u;
    
    order[0]=((0^dirX)<<2)|((0^dirY)<<1)|(0^dirZ);
    order[1]=((0^dirX)<<2)|((0^dirY)<<1)|(1^dirZ);
    order[2]=((0^dirX)<<2)|((1^dirY)<<1)|(0^dirZ);
    order[3]=((0^dirX)<<2)|((1^dirY)<<1)|(1^dirZ);
    order[4]=((1^dirX)<<2)|((0^dirY)<<1)|(0^dirZ);
    order[5]=((1^dirX)<<2)|((0^dirY)<<1)|(1^dirZ);
    order[6]=((1^dirX)<<2)|((1^dirY)<<1)|(0^dirZ);
    order[7]=((1^dirX)<<2)|((1^dirY)<<1)|(1^dirZ);
    
    #define VISIT_CHILD(i){\
      uint childOctant=order[i];\
      uint childMask=1u<<childOctant;\
      if((children&childMask)!=0u){\
        vec3 offset=vec3((childOctant>>2)&1,(childOctant>>1)&1,(childOctant>>0)&1);\
        float childSize=float(size/2);\
        vec3 childMin=entry.Min+offset*childSize;\
        vec3 childMax=childMin+childSize;\
        float tMin=0.;\
        if(intersectAABB(origin,direction,childMin,childMax,tMin)&&tMin<=dist){\
          stack[stackPtr++]=StackEntry(tMin,voxel.ChildIndex+bitCount(children&(childMask-1)),childMin);\
        }\
      }\
    }
    
    VISIT_CHILD(0);
    VISIT_CHILD(1);
    VISIT_CHILD(2);
    VISIT_CHILD(3);
    VISIT_CHILD(4);
    VISIT_CHILD(5);
    VISIT_CHILD(6);
    VISIT_CHILD(7);
  }
  
  StackEntry entry=stack[stackPtr];
  payload.TMin=entry.TMin;
  payload.NodeMin=entry.Min;
  payload.NodeIndex=offset+entry.Index;
  
  return payload;
}

float tNextBoundary(vec3 origin,vec3 direction){
  vec3 invDir=1./direction;
  
  // Next boundary depending on direction
  vec3 nextBoundary=vec3(0.);
  
  // For each axis: choose next multiple of 64
  for(int i=0;i<3;i++){
    if(direction[i]>0.){
      nextBoundary[i]=floor(origin[i]/64.)*64.+64.;
    }else{
      nextBoundary[i]=floor(origin[i]/64.)*64.;
    }
  }
  
  // Compute t for each axis
  vec3 tVals=(nextBoundary-origin)*invDir;
  
  // Ignore negative or invalid directions
  if(direction.x==0.)tVals.x=1e30;
  if(direction.y==0.)tVals.y=1e30;
  if(direction.z==0.)tVals.z=1e30;
  
  // Find smallest t
  float tMin=tVals.x;
  
  if(tVals.y<tMin)
  tMin=tVals.y;
  
  if(tVals.z<tMin)
  tMin=tVals.z;
    
  return tMin;
}

Hit raymarch(vec3 origin,vec3 direction,float dist)
{
  Hit payload;
  payload.IsValid=false;
  payload.TMin=0.;
  
  RaymarchStackEntry stack[RAYMARCH_MAX_STACK];  
  uint id=getChunkId(origin);

  uint ptr=0;
  stack[ptr++]=RaymarchStackEntry(id,0.,mod(origin,64.));
  
  while(ptr>0){
    RaymarchStackEntry entry=stack[--ptr];
    uint offset=headers.data[entry.ChunkId].Offset;
    
    payload=raymarchVoxels(offset,entry.Origin,direction,dist);
    if(payload.IsValid)return payload;
    
    float tStep = tNextBoundary(entry.Origin, direction);
    float tGlobal = entry.TMin + tStep + (0.1);

    vec3 globalPos = origin + direction * tGlobal;
    
    uint nId=getChunkId(globalPos);
    vec3 nOrigin=mod(globalPos,64.);
    stack[ptr++] = RaymarchStackEntry(nId, tGlobal, nOrigin);
  }
  
  return payload;
}