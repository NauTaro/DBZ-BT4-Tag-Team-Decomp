/* Only the final filtered pose is rendered. Neither obstacle candidates nor
 * actor teleports may directly replace the eye or its orientation. */
static int sSmoothReady;
static float sFocus[4],sEyeVelocity[3],sFocusVelocity[3],sYawVelocity,sPitchVelocity;
static int sc_blocked(const float *target,const float *eye);
static int sc_camera_blocked(const float *target,const float *eye);
void PortSpectator_ResetSmoothing(void){
    sSmoothReady=0;memset(sEyeVelocity,0,sizeof(sEyeVelocity));memset(sFocusVelocity,0,sizeof(sFocusVelocity));
    sYawVelocity=sPitchVelocity=0;sObstacleLift=0;sObstacleUntil=0;
    sSightYaw=sSightLift=0;sSightSince=0;
    sShotSpan=0;sShotZoom=sShotZoomGoal=1.04f;sShotZoomVelocity=0;
    sRouteY=0;sRouteUntil=0;sRouteFrame=0;
    sf_reset();
}
static void sm_limit(float *v,float limit){
    float length=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(length>limit)for(int i=0;i<3;++i)v[i]*=limit/length;
}
static void sm_step(float *value,float *velocity,const float *goal,float dt,float speed,float acceleration,float frequency){
    float a[3];
    for(int i=0;i<3;++i)a[i]=frequency*frequency*(goal[i]-value[i])-2*frequency*velocity[i];
    sm_limit(a,acceleration);
    for(int i=0;i<3;++i)velocity[i]+=a[i]*dt;
    sm_limit(velocity,speed);
    for(int i=0;i<3;++i)value[i]+=velocity[i]*dt;
}
static void sm_angle(float *angle,float *velocity,float goal,float dt,float speed,float acceleration){
    float error=atan2f(sinf(goal-*angle),cosf(goal-*angle));
    float wanted=fmaxf(-speed,fminf(speed,error*4.2f));
    *velocity+=fmaxf(-acceleration*dt,fminf(acceleration*dt,wanted-*velocity));
    float step=*velocity*dt;
    if(step*error>0 && fabsf(step)>fabsf(error)){step=error;*velocity=0;}
    *angle=atan2f(sinf(*angle+step),cosf(*angle+step));
}
int PortSpectator_FilterPose(float *eye,const float *look,float *rotation){
    float dt=sCameraDt,previous[4],goal[4];int first=!sSmoothReady,blocked=0;
    for(int i=0;i<3;++i)if(!isfinite(eye[i])||!isfinite(look[i]))return 0;
    memcpy(goal,eye,16);memcpy(previous,sCameraPos,16);sRouteFrame=0;
    if(first){
        memcpy(sCameraPos,eye,16);memcpy(sFocus,look,16);sSmoothReady=1;
        float dx=sFocus[0]-eye[0],dy=sFocus[1]-eye[1],dz=sFocus[2]-eye[2];
        sCameraRot[1]=atan2f(dx,dz);sCameraRot[0]=-atan2f(dy,sqrtf(dx*dx+dz*dz));
        sf_depthGuard(sCameraPos,sCameraRot,5);
        sShotZoom=fminf(sShotZoomGoal,sf_lensLimit(sCameraPos,sCameraRot)/sCameraZoom);
    }else if(dt>0){
        /* Translate with the combat's movement before the damped framing
         * correction. This keeps a fast exchange from outrunning the rig. */
        for(int k=0;k<3;++k){sCameraPos[k]+=sTrackDelta[k];sFocus[k]+=sTrackDelta[k];}
        /* Plan around a rock before reaching its surface. A clear end point
         * alone does not imply that the path to it is clear. First climb at
         * the current position, then traverse above the obstacle. */
        Uint64 now=sf_clock();
        if(sc_camera_blocked(previous,goal)||sf_bodySweep(previous,goal)){
            const float lifts[]={24,48,80,128,192,288,432,640};
            for(unsigned n=0;n<sizeof(lifts)/sizeof(lifts[0]);++n){
                float rise[4]={previous[0],fminf(previous[1],goal[1])-lifts[n],previous[2],1};
                float over[4]={goal[0],rise[1],goal[2],1};
                if(!sc_camera_blocked(previous,rise)&&!sc_camera_blocked(rise,over)&&!sf_bodySweep(previous,rise)&&!sf_bodySweep(rise,over)){
                    if(!sRouteUntil||rise[1]<sRouteY)sRouteY=rise[1];
                    sRouteUntil=now+1500000000ull;break;
                }
            }
        }
        if(now<sRouteUntil){
            goal[1]=fminf(goal[1],sRouteY);sRouteFrame=1;
            if(sc_camera_blocked(previous,goal)||sf_bodySweep(previous,goal)){goal[0]=previous[0];goal[2]=previous[2];}
        }else sRouteUntil=0;
        sf_orbitWaypoint(previous,look,goal,dt);
        float speed=sCameraSpeed/100.0f;
        sm_step(sCameraPos,sEyeVelocity,goal,dt,640*speed,2200*speed,5);
        sm_step(sFocus,sFocusVelocity,look,dt,1000*speed,4000*speed,7);
        sf_depthGuard(sCameraPos,sCameraRot,dt);
        /* A short segment test prevents the smoothed eye from crossing a wall.
         * A blocked horizontal move may continue vertically; it never jumps. */
        if(sc_camera_blocked(previous,sCameraPos)||sf_bodySweep(previous,sCameraPos)){
            float vertical[4]={previous[0],sCameraPos[1],previous[2],1};blocked=1;
            if(!sc_camera_blocked(previous,vertical)&&!sf_bodySweep(previous,vertical)){sCameraPos[0]=previous[0];sCameraPos[2]=previous[2];sEyeVelocity[0]=sEyeVelocity[2]=0;}
            else{memcpy(sCameraPos,previous,16);memset(sEyeVelocity,0,sizeof(sEyeVelocity));}
            /* Raise the goal above a wall until the actual eye can pass it.
             * This remains an acceleration-limited move, never a position cut. */
            float needed=goal[1]-previous[1]+32;
            if(needed>0)sObstacleLift=fminf(640,sObstacleLift+needed);
            sObstacleUntil=sf_clock()+2000000000ull;
        }
        float dx=sFocus[0]-sCameraPos[0],dy=sFocus[1]-sCameraPos[1],dz=sFocus[2]-sCameraPos[2];
        sm_angle(&sCameraRot[1],&sYawVelocity,atan2f(dx,dz),dt,2.6f,10);
        sm_angle(&sCameraRot[0],&sPitchVelocity,-atan2f(dy,sqrtf(dx*dx+dz*dz)),dt,1.6f,6);
        float wanted=fminf(sShotZoomGoal,sf_lensLimit(sCameraPos,sCameraRot)/sCameraZoom);
        float frequency=wanted<sShotZoom?24:4;
        float brake=sShotZoomVelocity<-.1f?96:.8f;
        float lensAcceleration=sf_clamp(frequency*frequency*(wanted-sShotZoom)-2*frequency*sShotZoomVelocity,-96,brake);
        sShotZoomVelocity=sf_clamp(sShotZoomVelocity+lensAcceleration*dt,-8,.35f);
        sShotZoom=sf_clamp(sShotZoom+sShotZoomVelocity*dt,1.0f,1.6f);
    }
    memcpy(eye,sCameraPos,16);memcpy(rotation,sCameraRot,16);eye[3]=1;rotation[2]=0;rotation[3]=sCameraZoom*sShotZoom;
    sf_recordCoverage(eye,rotation,rotation[3]);
    if(getenv("BT4_RENDER_TEST")){
        static unsigned frame;static FILE *csv;static int opened;
        if(!opened){const char *path=getenv("BT4_SPECTATOR_POSE_CSV");opened=1;if(path){csv=fopen(path,"w");if(csv)fprintf(csv,"frame,dt,first,x,y,z,focus_x,focus_y,focus_z,pitch,yaw,blocked,HUD,zoom,span,route,body_coverage,effect_coverage,power,effects\n");}}
        if(csv){fprintf(csv,"%u,%.7f,%d,%.7f,%.7f,%.7f,%.7f,%.7f,%.7f,%.7f,%.7f,%d,%d,%.7f,%.7f,%d,%.7f,%.7f,%d,%d\n",frame,dt,first,eye[0],eye[1],eye[2],sFocus[0],sFocus[1],sFocus[2],rotation[0],rotation[1],blocked,PortHud_Hidden(),rotation[3],sShotSpan,sRouteFrame,sFrameCoverage,sEffectCoverage,sPowerPhase,sEffectCount);if(frame%30==0)fflush(csv);}
        if(frame++%120==0)fprintf(stderr,"spectator camera: mode=3 count=%d pos=%.2f,%.2f,%.2f source=independent HUD=%d motion=filtered\n",sCameraCount,eye[0],eye[1],eye[2],PortHud_Hidden());
    }
    return 1;
}
