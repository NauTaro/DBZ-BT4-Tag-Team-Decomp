/* Camera controls are presentation state. They never enter room input packets. */
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>
extern int Port_NetActive(void),TagBattle_Network(void),gPortOverlayOpen;
static int sCameraMode,sCameraTarget=1,sCameraCount=2,sCameraSpeed=100,sCameraReady,sCameraFixture;
static float sCameraPos[4],sCameraRot[4],sCameraOffset[3],sCameraZoom=1.0f,sOverviewAngle=.75f;
static Uint64 sCameraTime;
static Uint64 sFramingClock;
static Uint64 sf_clock(void){return sFramingClock?sFramingClock:SDL_GetTicksNS();}
static float sCameraDt,sObstacleLift;
static Uint64 sObstacleUntil;
static float sSightYaw,sSightLift;
static Uint64 sSightSince;
static float sShotSpan,sShotZoomGoal=1.04f,sShotZoom=1.04f,sShotZoomVelocity;
static float sRouteY;
static Uint64 sRouteUntil;
static int sRouteFrame;
static float sCameraLook[4];
static float sCameraAnchor[4],sAvoidY;
static Uint64 sAvoidUntil;
static int sDirectorActor=1;
#include "spectator_framing.inc"
void PortSpectator_SetCutOwner(int actor){if(actor>=0&&actor<sCameraCount)sDirectorActor=actor;}
int PortSpectator_GetLook(float *out){if(sCameraMode<2)return 0;memcpy(out,sCameraLook,16);return 1;}
void PortSpectator_GetAnchor(float *out){memcpy(out,sCameraAnchor,16);}
void PortSpectator_ResolveEye(const float *eye){memcpy(sCameraPos,eye,16);sAvoidY=eye[1];sAvoidUntil=SDL_GetTicksNS()+1200000000ull;}
int PortSpectator_CanPreview(void){return !Port_NetActive()&&!TagBattle_Network();}
int PortSpectator_Mode(void){
    if(!sCameraFixture){sCameraFixture=1;
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_SPECTATOR_CAMERA_TEST"))
            PortSpectator_SetMode(atoi(getenv("BT4_SPECTATOR_CAMERA_TEST")));
    }
    return PortSpectator_CanPreview()?sCameraMode:0;
}
int PortSpectator_SetMode(int mode){
    if(mode<0||mode>3||!PortSpectator_CanPreview())return 0;
    sCameraMode=mode;sCameraReady=0;sCameraTime=0;sAvoidUntil=0;PortSpectator_ResetSmoothing();return 1;
}
void PortSpectator_ResetCamera(void){sCameraReady=0;sCameraTime=0;sCameraZoom=1.0f;sDirectorActor=1;sAvoidUntil=0;PortSpectator_ResetSmoothing();}
void PortSpectator_SetTarget(int actor){sCameraTarget=actor<0?0:actor>9?9:actor;sCameraReady=0;PortSpectator_ResetSmoothing();}
int PortSpectator_Target(void){return sCameraTarget;}
int PortSpectator_Count(void){return sCameraCount;}
void PortSpectator_SetSpeed(int percent){sCameraSpeed=percent<25?25:percent>400?400:percent;}
int PortSpectator_Speed(void){return sCameraSpeed;}
static float camera_axis(SDL_Gamepad *pad,SDL_GamepadAxis axis){
    float f=pad?SDL_GetGamepadAxis(pad,axis)/32767.0f:0;
    return fabsf(f)<.16f?0:f;
}
int PortSpectator_Camera(const float *basePos,const float *baseRot,const float *targets,int count,float *pos,float *rot){
    int i,first=!sCameraReady;float dt,forward=0,side=0,up=0,yaw=0,pitch=0,zoom=0,boost=1;
    Uint64 now=SDL_GetTicksNS();
    if(!sCameraFixture){sCameraFixture=1;
        if(getenv("BT4_RENDER_TEST")&&getenv("BT4_SPECTATOR_CAMERA_TEST"))
            PortSpectator_SetMode(atoi(getenv("BT4_SPECTATOR_CAMERA_TEST")));
    }
    if(!PortSpectator_Mode())return 0;
    if(count<1||count>10)return 0;
    sCameraCount=count;if(sCameraTarget>=count)sCameraTarget=count-1;
    dt=sCameraTime?(float)(now-sCameraTime)/1000000000.0f:0.0f;
    sCameraTime=now;if(dt>.05f)dt=.05f;if(dt<0)dt=0;
    sCameraDt=dt;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_SPECTATOR_FAST_TEST")){sCameraDt=dt=1.0f/30;sFramingClock+=33333333ull;}
    else sFramingClock=now;
    if(!sCameraReady){
        if(sCameraMode!=3){
            memcpy(sCameraPos,basePos,16);memcpy(sCameraRot,baseRot,16);
            for(i=0;i<3;++i)sCameraOffset[i]=basePos[i]-targets[sCameraTarget*4+i];
        }else{memset(sCameraPos,0,16);memset(sCameraRot,0,16);}
        sCameraReady=1;
    }
    if(SDL_GetKeyboardFocus()&&!gPortOverlayOpen&&!getenv("BT4_RENDER_TEST")){
        const bool *keys=SDL_GetKeyboardState(NULL);
        forward=(float)keys[SDL_SCANCODE_W]-keys[SDL_SCANCODE_S];
        side=(float)keys[SDL_SCANCODE_D]-keys[SDL_SCANCODE_A];
        up=(float)keys[SDL_SCANCODE_E]-keys[SDL_SCANCODE_Q];
        yaw=(float)keys[SDL_SCANCODE_RIGHT]-keys[SDL_SCANCODE_LEFT];
        pitch=(float)keys[SDL_SCANCODE_DOWN]-keys[SDL_SCANCODE_UP];
        zoom=(float)keys[SDL_SCANCODE_EQUALS]-keys[SDL_SCANCODE_MINUS];
        if(keys[SDL_SCANCODE_LSHIFT]||keys[SDL_SCANCODE_RSHIFT])boost=3;
        int n=0;SDL_JoystickID *ids=SDL_GetGamepads(&n);
        SDL_Gamepad *pad=n?SDL_GetGamepadFromID(ids[0]):NULL;
        side+=camera_axis(pad,SDL_GAMEPAD_AXIS_LEFTX);
        forward-=camera_axis(pad,SDL_GAMEPAD_AXIS_LEFTY);
        yaw+=camera_axis(pad,SDL_GAMEPAD_AXIS_RIGHTX);
        pitch+=camera_axis(pad,SDL_GAMEPAD_AXIS_RIGHTY);
        if(pad){up+=SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)-SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);}
        SDL_free(ids);
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_SPECTATOR_CAMERA_MOVE_TEST")){side=.3f;yaw=.08f;}
    if(sCameraMode!=3){
        sCameraRot[1]+=yaw*dt*1.5f;sCameraRot[0]+=pitch*dt*1.2f;
        if(sCameraRot[0]>1.4f)sCameraRot[0]=1.4f;if(sCameraRot[0]<-1.4f)sCameraRot[0]=-1.4f;
    }
    sCameraZoom+=zoom*dt;if(sCameraZoom<.4f)sCameraZoom=.4f;if(sCameraZoom>2)sCameraZoom=2;
    if(sCameraMode==1){
        float speed=240.0f*(sCameraSpeed/100.0f)*boost*dt;
        float sy=sinf(sCameraRot[1]),cy=cosf(sCameraRot[1]);
        sCameraPos[0]+=(forward*sy+side*cy)*speed;
        sCameraPos[2]+=(forward*cy-side*sy)*speed;
        sCameraPos[1]-=up*speed;
    }else if(sCameraMode==2){
        const float *target=targets+sCameraTarget*4;
        float a=1.0f-expf(-dt*9.0f);
        for(i=0;i<3;++i)sCameraPos[i]+=(target[i]+sCameraOffset[i]-sCameraPos[i])*a;
        float dx=target[0]-sCameraPos[0],dy=target[1]-10-sCameraPos[1],dz=target[2]-sCameraPos[2];
        for(i=0;i<3;++i)sCameraLook[i]=target[i];sCameraLook[1]-=10;sCameraLook[3]=1;
        memcpy(sCameraAnchor,sCameraLook,16);
        sCameraRot[1]=atan2f(dx,dz);sCameraRot[0]=-atan2f(dy,sqrtf(dx*dx+dz*dz));sCameraRot[2]=0;
    }else{
        sf_begin(targets,count);sf_goal(first,yaw,pos,rot);return 1;

    }
    for(i=0;i<3;++i){if(!isfinite(sCameraPos[i])||fabsf(sCameraPos[i])>100000.0f){sCameraReady=0;return 0;}}
    memcpy(pos,sCameraPos,16);memcpy(rot,sCameraRot,16);pos[3]=1;rot[3]=sCameraZoom;
    if(getenv("BT4_RENDER_TEST")){
        static unsigned frames;if((frames++%120)==0)
            fprintf(stderr,"spectator camera: mode=%d target=%d count=%d pos=%.2f,%.2f,%.2f zoom=%.2f source=%s HUD=%d\n",sCameraMode,sCameraTarget,count,pos[0],pos[1],pos[2],sCameraZoom,sCameraMode==3?"independent":"local",PortHud_Hidden());
    }
    return 1;
}
#include "spectator_smoothing.inc"
#include "spectator_collision.inc"
