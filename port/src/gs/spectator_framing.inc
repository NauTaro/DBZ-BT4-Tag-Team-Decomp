/* Screen-space composition for the independent observer. All inputs are
 * read-only samples; the director never edits fighters or projectile tasks. */
static float sBodies[40],sBodyPrevious[40],sBodyLead[40],sEffects[64];
static int sBodyCount,sEffectCount,sSceneProvided,sBodiesPreviousReady,sPowerPhase;
static float sProjectionX=1.087f,sProjectionY=1.933f,sFrameCoverage,sEffectCoverage;
static unsigned sRestoredKi;
static float sTrackDelta[3];
void PortSpectator_NoteEffect(int obj){
    ++sRestoredKi;
    if(getenv("BT4_RENDER_TEST")&&(sRestoredKi<16||sRestoredKi%500==0))fprintf(stderr,"spectator effects: live-ki-visible object=%d restored=%u\n",obj,sRestoredKi);
}
static float sf_clamp(float x,float a,float b){return fmaxf(a,fminf(b,x));}
static void sf_reset(void){sBodyCount=sEffectCount=sSceneProvided=sBodiesPreviousReady=sPowerPhase=0;sFrameCoverage=sEffectCoverage=0;memset(sTrackDelta,0,sizeof(sTrackDelta));}
void PortSpectator_SetScene(const float *body,int count,const float *fx,int fxCount,int phase,float px,float py){
    int next=count<0?0:count>10?10:count;sEffectCount=fxCount<0?0:fxCount>16?16:fxCount;
    if(body&&next){sBodyCount=next;memcpy(sBodies,body,sBodyCount*16);}
    if(fx)memcpy(sEffects,fx,sEffectCount*16);else sEffectCount=0;
    sPowerPhase=phase;sProjectionX=sf_clamp(px,.1f,8);sProjectionY=sf_clamp(py,.1f,8);sSceneProvided=1;
}
static void sf_begin(const float *targets,int count){
    if(!sSceneProvided){sBodyCount=count;sEffectCount=sPowerPhase=0;
        for(int i=0;i<count;++i){memcpy(sBodies+i*4,targets+i*4,16);sBodies[i*4+1]-=10;sBodies[i*4+3]=11;}}
    sSceneProvided=0;
    float trackingLength=0;
    for(int k=0;k<3;++k){sTrackDelta[k]=0;
        if(sBodiesPreviousReady)for(int i=0;i<sBodyCount;++i)sTrackDelta[k]+=(sBodies[i*4+k]-sBodyPrevious[i*4+k])/sBodyCount;
        trackingLength+=sTrackDelta[k]*sTrackDelta[k];}
    trackingLength=sqrtf(trackingLength);
    float trackingLimit=600*fmaxf(sCameraDt,0);
    if(trackingLength>trackingLimit && trackingLength>0)for(int k=0;k<3;++k)sTrackDelta[k]*=trackingLimit/trackingLength;
    for(int i=0;i<sBodyCount;++i){float delta[3],length=0;
        for(int k=0;k<3;++k){delta[k]=sBodiesPreviousReady?(sBodies[i*4+k]-sBodyPrevious[i*4+k])*.10f/fmaxf(sCameraDt,.016f):0;length+=delta[k]*delta[k];}
        length=sqrtf(length);if(length>36)for(int k=0;k<3;++k)delta[k]*=36/length;
        for(int k=0;k<3;++k)sBodyLead[i*4+k]=sBodies[i*4+k]+delta[k];sBodyLead[i*4+3]=sBodies[i*4+3];
    }
    memcpy(sBodyPrevious,sBodies,sizeof(sBodies));sBodiesPreviousReady=1;
}
static void sf_depthGuard(float *eye,const float *rotation,float dt){
    float sy=sinf(rotation[1]),cy=cosf(rotation[1]),sp=sinf(rotation[0]),cp=cosf(rotation[0]);
    float f[3]={sy*cp,-sp,cy*cp},need=0;
    for(int i=0;i<sBodyCount+sEffectCount;++i)for(int sample=0;sample<2;++sample){
        const float *p=i<sBodyCount?(sample?sBodyLead:sBodies)+i*4:sEffects+(i-sBodyCount)*4;
        float dx=p[0]-eye[0],dy=p[1]-eye[1],dz=p[2]-eye[2],r=fabsf(p[3]);
        float x=cy*dx-sy*dz,y=sy*sp*dx+cp*dy+cy*sp*dz,z=dx*f[0]+dy*f[1]+dz*f[2];
        float projection=fmaxf(sProjectionX*(fabsf(x)+r*.85f),sProjectionY*(fabsf(y)+r*1.2f));
        need=fmaxf(need,r+24-z);need=fmaxf(need,r+projection/.60f+4-z);}
    need=fminf(need,1200*dt);
    for(int k=0;k<3;++k)eye[k]-=f[k]*need;
}
static void sf_goal(int first,float yaw,float *pos,float *rot){
    float low[3]={1e20f,1e20f,1e20f},high[3]={-1e20f,-1e20f,-1e20f};
    float center[3],radius=0,span=0;
    for(int i=0;i<sBodyCount;++i){float r=sf_clamp(fabsf(sBodies[i*4+3]),3,100);
        for(int k=0;k<3;++k){float pad=r*(k==1?1.2f:.85f);
            low[k]=fminf(low[k],fminf(sBodies[i*4+k],sBodyLead[i*4+k])-pad);
            high[k]=fmaxf(high[k],fmaxf(sBodies[i*4+k],sBodyLead[i*4+k])+pad);}
        for(int j=i+1;j<sBodyCount;++j){float d=0;for(int k=0;k<3;++k){float x=sBodies[i*4+k]-sBodies[j*4+k];d+=x*x;}span=fmaxf(span,sqrtf(d));}
    }
    /* A missed blast leaving the arena should not pull the broadcast away.
     * Keep live effects near the volume containing the combatants. */
    int kept=0;
    for(int i=0;i<sEffectCount;++i){int good=1;
        for(int k=0;k<3;++k)if(!isfinite(sEffects[i*4+k])||sEffects[i*4+k]<low[k]-100||sEffects[i*4+k]>high[k]+100)good=0;
        if(!good)continue;
        if(kept!=i)memcpy(sEffects+kept*4,sEffects+i*4,16);++kept;
    }
    sEffectCount=kept;
    for(int i=0;i<sEffectCount;++i)for(int k=0;k<3;++k){float r=sf_clamp(sEffects[i*4+3],2,24);low[k]=fminf(low[k],sEffects[i*4+k]-r);high[k]=fmaxf(high[k],sEffects[i*4+k]+r);}
    for(int k=0;k<3;++k){center[k]=(low[k]+high[k])*.5f;float d=(high[k]-low[k])*.5f;radius+=d*d;}
    radius=sqrtf(radius);sShotSpan=span;
    float close=1-sf_clamp((span-32)/110,0,1);
    sShotZoomGoal=sPowerPhase==2?1.15f:sPowerPhase==1?1.50f:1.0f+.55f*close;
    if(sBodyCount==2){float dx=sBodies[4]-sBodies[0],dz=sBodies[6]-sBodies[2];
        if(dx*dx+dz*dz>64){
            float angle=atan2f(dz,-dx),delta=atan2f(sinf(angle-sOverviewAngle),cosf(angle-sOverviewAngle));
            if(fabsf(delta)>1.5707963f)angle+=3.14159265f;
            delta=atan2f(sinf(angle-sOverviewAngle),cosf(angle-sOverviewAngle));
            if(first)sOverviewAngle=angle;
            /* Hold the initial side of the action. Changing the line's side
             * while two fighters cross can otherwise induce a full orbit. */
        }
    }
    radius=sf_clamp(radius*1.55f+8,40,2400);sOverviewAngle+=yaw*sCameraDt;
    pos[0]=center[0]+sinf(sOverviewAngle)*radius;pos[1]=center[1]-3-radius*.16f;pos[2]=center[2]+cosf(sOverviewAngle)*radius;pos[3]=1;
    for(int k=0;k<3;++k)sCameraLook[k]=center[k];sCameraLook[3]=1;
    memcpy(sCameraAnchor,sBodies+(sDirectorActor<sBodyCount?sDirectorActor:0)*4,16);
    memcpy(rot,sCameraRot,16);rot[3]=sCameraZoom;
}
static float sf_projectLimit(const float *eye,const float *rotation,const float *points,int n,int body,float margin,float *coverage,float zoom){
    float sy=sinf(rotation[1]),cy=cosf(rotation[1]),sp=sinf(rotation[0]),cp=cosf(rotation[0]),limit=4;
    *coverage=0;
    for(int i=0;i<n;++i){const float *p=points+i*4;if(body==1 && p[3]<0)continue;float dx=p[0]-eye[0],dy=p[1]-eye[1],dz=p[2]-eye[2];
        float x=cy*dx-sy*dz,y=sy*sp*dx+cp*dy+cy*sp*dz,z=sy*cp*dx-sp*dy+cy*cp*dz;
        float r=sf_clamp(fabsf(p[3]),2,100),rx=r*(body?.85f:1),ry=r*(body?1.2f:1);
        if(z<=rx+1){limit=.08f;*coverage=10000;continue;}
        float a=sProjectionX*(fabsf(x)+rx)/(z-rx),b=sProjectionY*(fabsf(y)+ry)/(z-rx);
        limit=fminf(limit,margin/fmaxf(a,b));*coverage=fmaxf(*coverage,fmaxf(a,b)*zoom);
    }
    return limit;
}
static float sf_lensLimit(const float *eye,const float *rot){
    float a,b,limit=sf_projectLimit(eye,rot,sBodies,sBodyCount,2,.58f,&a,1);
    limit=fminf(limit,sf_projectLimit(eye,rot,sEffects,sEffectCount,0,.72f,&b,1));
    return sf_clamp(limit,1.0f,2);
}
static void sf_orbitWaypoint(const float *previous,const float *look,float *goal,float dt){
    float ax=previous[0]-look[0],az=previous[2]-look[2],bx=goal[0]-look[0],bz=goal[2]-look[2];
    float ar=hypotf(ax,az),br=hypotf(bx,bz);
    if(ar<20||br<20)return;
    float angle=atan2f(ax,az),target=atan2f(bx,bz),delta=atan2f(sinf(target-angle),cosf(target-angle));
    if(fabsf(delta)>.4f){
        float next=angle+sf_clamp(delta,-.8f*dt,.8f*dt),radius=fmaxf(ar,br);
        goal[0]=look[0]+sinf(next)*radius;goal[2]=look[2]+cosf(next)*radius;
    }
}
static void sf_recordCoverage(const float *eye,const float *rot,float zoom){
    sf_projectLimit(eye,rot,sBodies,sBodyCount,1,1,&sFrameCoverage,zoom);
    sf_projectLimit(eye,rot,sEffects,sEffectCount,0,1,&sEffectCoverage,zoom);
}
static int sf_bodySweep(const float *a,const float *b){
    float d[3],len=0;for(int k=0;k<3;++k){d[k]=b[k]-a[k];len+=d[k]*d[k];}
    for(int i=0;i<sBodyCount;++i){const float *p=sBodies+i*4;if(p[3]<0)continue;float t=0,dist=0,start=0,r=p[3]+3;
        for(int k=0;k<3;++k){t+=(p[k]-a[k])*d[k];start+=(p[k]-a[k])*(p[k]-a[k]);}
        t=len>1e-8f?sf_clamp(t/len,0,1):0;
        for(int k=0;k<3;++k){float x=a[k]+d[k]*t-p[k];dist+=x*x;}
        if(start>r*r && dist<r*r)return 1;
    }
    return 0;
}
