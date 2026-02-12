struct FlatNode{
  uint PackedIDC;
  uint ChildIndex;
};

layout(std430,set=1,binding=50)readonly buffer SparseVoxelOctree{
  uint count;
  uint padding[3];
  FlatNode data[];
}voxels;

struct StackEntry{
  float TMin;
  uint Index;
  vec3 Min;
};

const uint MAX_STACK=24;

Hit raymarch(vec3 origin,vec3 direction,float dist)
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
    
    FlatNode voxel=voxels.data[entry.Index];
    
    uint depth=0xFFu&(voxel.PackedIDC>>8);
    
    uint children=0xFFu&voxel.PackedIDC;
    
    uint size=(1<<depth);
    
    if(children==0u){
      uint id=0xFFFFu&(voxel.PackedIDC>>16);
      
      payload.IsValid=true;
      payload.TMin=entry.TMin;
      payload.NodeMin=entry.Min;
      payload.NodeSize=size;
      payload.NodeIndex=entry.Index;
      payload.Material=id;
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
  
  return payload;
}