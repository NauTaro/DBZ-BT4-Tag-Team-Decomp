/* Observer-only ray queries. Read the whole collision BVH instead of the
 * fighter camera's zone-limited sweep. No fighter or scripted camera is used. */
#include <stdint.h>
typedef struct ScMesh {int nodes,polys,unused8,nodeOfs,polyOfs,vtxOfs;} ScMesh;
typedef struct ScNode {float min[3],max[3];int left,right;} ScNode;
typedef struct ScPoly {unsigned flags;int vertex[3];float plane[4];} ScPoly;
/* These globals belong to the recompiled game's pointer32 memory. The host
 * renderer is 64-bit; reading either symbol as a host pointer joins two words. */
typedef struct ScStage {unsigned char header[0x38];uint32_t zoneCount,zones,bounds,objCount,objs;} ScStage;
typedef struct ScZone {int mark;uint32_t mesh;int nearCount;uint32_t near;int unused10,recCount;uint32_t recs;int unused1C;float center[4],half[4];} ScZone;
typedef struct ScRec {int broken,obj;uint32_t mesh;int unused;} ScRec;
extern uint32_t gBtlStage,gColMeshBase;
static int sc_box(const float *origin,const float *delta,const ScNode *node){
    float lo=.002f,hi=.998f;
    for(int a=0;a<3;++a){
        if(fabsf(delta[a])<1e-7f){if(origin[a]<node->min[a]||origin[a]>node->max[a])return 0;}
        else{float x=(node->min[a]-origin[a])/delta[a],y=(node->max[a]-origin[a])/delta[a];
            if(x>y){float t=x;x=y;y=t;}if(x>lo)lo=x;if(y<hi)hi=y;if(lo>hi)return 0;}
    }
    return 1;
}
static int sc_triangle(const float *o,const float *d,const float *a,const float *b,const float *c){
    float e[3],f[3],p[3],q[3],t[3];
    for(int k=0;k<3;++k){e[k]=b[k]-a[k];f[k]=c[k]-a[k];t[k]=o[k]-a[k];}
    p[0]=d[1]*f[2]-d[2]*f[1];p[1]=d[2]*f[0]-d[0]*f[2];p[2]=d[0]*f[1]-d[1]*f[0];
    float det=e[0]*p[0]+e[1]*p[1]+e[2]*p[2];if(fabsf(det)<1e-6f)return 0;
    float u=(t[0]*p[0]+t[1]*p[1]+t[2]*p[2])/det;if(u<0||u>1)return 0;
    q[0]=t[1]*e[2]-t[2]*e[1];q[1]=t[2]*e[0]-t[0]*e[2];q[2]=t[0]*e[1]-t[1]*e[0];
    float v=(d[0]*q[0]+d[1]*q[1]+d[2]*q[2])/det;if(v<0||u+v>1)return 0;
    float distance=(f[0]*q[0]+f[1]*q[1]+f[2]*q[2])/det;
    return distance>.002f&&distance<.998f;
}
static int sc_mesh_blocked(ScMesh *m,const float *target,const float *eye){
    int *base=(int*)(uintptr_t)gColMeshBase;
    if(!m||!base||m->nodes<1||m->nodes>500000||m->nodeOfs<0||m->nodeOfs>0x1000000||m->polyOfs<0||m->polyOfs>0x1000000||m->vtxOfs<0||m->vtxOfs>0x1000000)return 0;
    ScNode *nodes=(ScNode*)(base+m->nodeOfs);
    ScPoly *polys=(ScPoly*)(base+m->polyOfs);
    float *vertices=(float*)(base+m->vtxOfs),d[3];
    int stack[128],used=1,visits=0;stack[0]=0;
    for(int a=0;a<3;++a)d[a]=eye[a]-target[a];
    while(used && ++visits<m->nodes*2+2){
        int id=stack[--used];if(id<0||id>=m->nodes)continue;
        ScNode *n=nodes+id;if(!sc_box(target,d,n))continue;
        if(n->right<0){
            if(n->left<0||n->left>=m->polys)continue;
            ScPoly *p=polys+n->left;
            if((p->flags&0x08000000u)||p->vertex[0]<0||p->vertex[1]<0||p->vertex[2]<0||p->vertex[0]>1000000||p->vertex[1]>1000000||p->vertex[2]>1000000)continue;
            if(sc_triangle(target,d,vertices+p->vertex[0]*4,vertices+p->vertex[1]*4,vertices+p->vertex[2]*4))return 1;
        }else if(used<126){stack[used++]=n->left;stack[used++]=n->right;}
    }
    return 0;
}
static int sc_blocked(const float *target,const float *eye){
    ScStage *stage=(ScStage*)(uintptr_t)gBtlStage;
    if(!stage||!stage->zones||stage->zoneCount>16384)return 0;
    ScZone *zones=(ScZone*)(uintptr_t)stage->zones;
    for(unsigned z=0;z<stage->zoneCount;++z){
        ScZone *zone=zones+z;
        if(sc_mesh_blocked((ScMesh*)(uintptr_t)zone->mesh,target,eye))return 1;
        if(!zone->recs||zone->recCount<0||zone->recCount>4096)continue;
        ScRec *recs=(ScRec*)(uintptr_t)zone->recs;
        for(int r=0;r<zone->recCount;++r){
            ScRec *rec=recs+r;
            if(rec->obj<0||(unsigned)rec->obj>=stage->objCount||!stage->objs)continue;
            unsigned state=*(unsigned*)(uintptr_t)(stage->objs+rec->obj*0x50);
            if((rec->broken!=0)!=((state&1)!=0))continue;
            if(sc_mesh_blocked((ScMesh*)(uintptr_t)rec->mesh,target,eye))return 1;
        }
    }
    return 0;
}
static int sc_score(const float *targets,int count,const float *eye){
    int blocked=0;
    for(int a=0;a<count;++a){float target[3]={targets[a*4],targets[a*4+1]-9,targets[a*4+2]};blocked+=sc_blocked(target,eye);}
    return blocked;
}
static int sc_camera_blocked(const float *a,const float *b){
    if(sc_blocked(a,b))return 1;
    for(int axis=0;axis<3;++axis)for(int sign=-1;sign<=1;sign+=2){
        float from[4]={a[0],a[1],a[2],1},to[4]={b[0],b[1],b[2],1};
        from[axis]+=3*sign;to[axis]+=3*sign;
        if(sc_blocked(from,to))return 1;
    }
    return 0;
}
void PortSpectator_StageAvoid(const float *targets,int count,const float *look,float *eye){
    if(count<1||count>10)return;
    if(PortSpectator_Mode()==3){
        float base[4],held[4],bestEye[4];memcpy(base,eye,16);
        float dx=base[0]-look[0],dz=base[2]-look[2];
        Uint64 now=sf_clock();
        float cs=cosf(sSightYaw),sn=sinf(sSightYaw);
        held[0]=look[0]+cs*dx+sn*dz;held[2]=look[2]-sn*dx+cs*dz;
        held[1]=base[1]-sSightLift-sObstacleLift;held[3]=1;
        int blocked=sc_score(targets,count,held),bestBlocked=blocked;
        memcpy(bestEye,held,16);
        /* Keep a clear framing side. Reconsider a blocked shot only after a
         * short hold, so moving silhouettes cannot alternate camera targets. */
        if(!sSightSince || (blocked && now-sSightSince>600000000ull)){
            const float angles[]={0,.5235988f,-.5235988f,1.0471976f,-1.0471976f,1.5707963f,-1.5707963f,3.14159265f};
            const float lifts[]={0,24,48,80,128,192};
            float bestCost=1e30f,bestYaw=sSightYaw,bestLift=sSightLift;
            for(unsigned h=0;h<sizeof(lifts)/sizeof(lifts[0]);++h){
                for(unsigned a=0;a<sizeof(angles)/sizeof(angles[0]);++a){
                    cs=cosf(angles[a]);sn=sinf(angles[a]);
                    float candidate[4]={look[0]+cs*dx+sn*dz,base[1]-lifts[h]-sObstacleLift,look[2]-sn*dx+cs*dz,1};
                    int score=sc_score(targets,count,candidate);
                    float delta=atan2f(sinf(angles[a]-sSightYaw),cosf(angles[a]-sSightYaw));
                    if(sSmoothReady && fabsf(delta)>1.1f)continue;
                    float cost=score*1000000.0f+lifts[h]*4+fabsf(delta)*120;
                    if(sSmoothReady){float x=candidate[0]-sCameraPos[0],y=candidate[1]-sCameraPos[1],z=candidate[2]-sCameraPos[2];cost+=sqrtf(x*x+y*y+z*z)*.1f;}
                    if(cost<bestCost){bestCost=cost;bestBlocked=score;bestYaw=angles[a];bestLift=lifts[h];memcpy(bestEye,candidate,16);}
                }
            }
            sSightYaw=bestYaw;sSightLift=bestLift;sSightSince=now;
        }
        if(now>sObstacleUntil)sObstacleLift=fmaxf(0,sObstacleLift-16*sCameraDt);
        memcpy(eye,bestEye,16);
        /* Only framing goals leave this function. FilterPose owns the eye. */
        if(getenv("BT4_RENDER_TEST")){static int reports;if(reports++<16){ScStage *stage=(ScStage*)(uintptr_t)gBtlStage;
            fprintf(stderr,"spectator sightline: zones=%u remaining-blocked=%d side=%.3f lift-goal=%.1f\n",stage?stage->zoneCount:0,bestBlocked,sSightYaw,sSightLift+sObstacleLift);}}
        return;
    }
    int best=sc_score(targets,count,eye);float wanted=0;
    const float lifts[]={24,48,80,128,192,288,432,640};
    for(unsigned i=0;i<sizeof(lifts)/sizeof(lifts[0]) && best;++i){
        float candidate[4]={eye[0],eye[1]-lifts[i],eye[2],1};int score=sc_score(targets,count,candidate);
        if(score<best){best=score;wanted=lifts[i];}
    }
    Uint64 now=sf_clock();
    if(wanted>sObstacleLift){sObstacleLift=wanted;sObstacleUntil=now+2000000000ull;}
    else if(now>sObstacleUntil)sObstacleLift=fmaxf(wanted,sObstacleLift-28*sCameraDt);
    eye[1]-=sObstacleLift;
    if(PortSpectator_Mode()!=3)PortSpectator_ResolveEye(eye);
    if(getenv("BT4_RENDER_TEST")){
        ScStage *stage=(ScStage*)(uintptr_t)gBtlStage;
        static int reports;if(reports++<16)fprintf(stderr,"spectator sightline: zones=%u remaining-blocked=%d lift-goal=%.1f\n",stage?stage->zoneCount:0,best,sObstacleLift);
    }
}
