/* Read-only camera rays. Native StgCol queries overwrite shared collision
 * scratch globals, so local spectator cameras must not use those queries. */
#include "battle/col_a.h"
#define BT4_SPECTATOR_GAME_MATH
#include "../../port/src/gs/spectator_camera.h"
#undef BT4_SPECTATOR_GAME_MATH
typedef struct SpecStageRec { s32 broken,obj;ColMesh *mesh;s32 unused; } SpecStageRec;
typedef struct SpecStageZone {
    s32 mark;ColMesh *mesh;s32 nearCount;void *near;s32 unused,recCount;SpecStageRec *recs;s32 unused1;
    Vec4 center,half;
} SpecStageZone;
typedef struct SpecStageObject { s32 state;u8 unused[0x4c]; } SpecStageObject;
typedef struct SpecStage {
    u8 unused[0x38];u32 count;SpecStageZone *zones;void *bounds;u32 objectCount;SpecStageObject *objects;
} SpecStage;
extern SpecStage *gBtlStage;
static s32 SpecCamera_Box(const ColBox *box,const f32 *from,const f32 *to,f32 end) {
    f32 begin=0;int k;
    for(k=0;k<3;k++) {
        f32 d=to[k]-from[k],a,b,t;
        if(fabsf(d)<.000001f){if(from[k]<box->min[k]-1 || from[k]>box->max[k]+1)return 0;continue;}
        a=(box->min[k]-1-from[k])/d;b=(box->max[k]+1-from[k])/d;
        if(a>b){t=a;a=b;b=t;}if(a>begin)begin=a;if(b<end)end=b;if(begin>end)return 0;
    }
    return 1;
}
static void SpecCamera_Mesh(ColMesh *mesh,const f32 *from,const f32 *to,f32 *fraction) {
    s32 stack[128],count=1,visits=0;
    ColNode *nodes;ColPoly *polys;ColVec *vertices;
    if(!mesh || !gColMeshBase || mesh->nodeCount<=0)return;
    nodes=(ColNode *)(gColMeshBase+mesh->nodeOfs);polys=(ColPoly *)(gColMeshBase+mesh->polyOfs);
    vertices=(ColVec *)(gColMeshBase+mesh->vtxOfs);stack[0]=0;
    while(count && ++visits<=mesh->nodeCount) {
        s32 index=stack[--count];ColNode *node;
        if(index<0 || index>=mesh->nodeCount)continue;
        node=&nodes[index];
        if(!SpecCamera_Box(&node->box,from,to,*fraction))continue;
        if(node->right==-1) {
            ColPoly *poly;if(node->left<0)continue;poly=&polys[node->left];
            if(poly->flags&(0x0c000000|0x60))continue;
            spec_triangle(from,to,(f32 *)&vertices[poly->vtx[0]],(f32 *)&vertices[poly->vtx[1]],(f32 *)&vertices[poly->vtx[2]],fraction);
        } else if(count+2<=128) { stack[count++]=node->right;stack[count++]=node->left; }
    }
}
static f32 SpecCamera_StageRay(const f32 *from,const f32 *to) {
    f32 fraction=1;u32 i;int j;
    if(!gBtlStage || !gBtlStage->zones || (Battle_GetWork()->flags&BATTLE_FLAG_LOADING))return 1;
    for(i=0;i<gBtlStage->count;i++) {
        SpecStageZone *zone=&gBtlStage->zones[i];
        SpecCamera_Mesh(zone->mesh,from,to,&fraction);
        for(j=0;j<zone->recCount;j++) {
            SpecStageRec *rec=&zone->recs[j];
            if(rec->obj<0 || (u32)rec->obj>=gBtlStage->objectCount || !gBtlStage->objects)continue;
            if((rec->broken!=0)!=((gBtlStage->objects[rec->obj].state&1)!=0))continue;
            SpecCamera_Mesh(rec->mesh,from,to,&fraction);
        }
    }
    return fraction;
}
static void SpecCamera_AvoidStage(Vec4 *eye,const Vec4 *previous) {
    f32 hit=SpecCamera_StageRay((const f32 *)previous,(const f32 *)eye);
    if(hit<1) {
        f32 dx=eye->x-previous->x,dy=eye->y-previous->y,dz=eye->z-previous->z;
        f32 length=sqrtf(dx*dx+dy*dy+dz*dz);
        hit=length>0?fmaxf(0,hit-2/length):0;
        eye->x=previous->x+dx*hit;eye->y=previous->y+dy*hit;eye->z=previous->z+dz*hit;
    }
}
