/* Presentation-only battle snapshot; no audio callback reads the game heap. */
#include <SDL3/SDL.h>
#include <string.h>
#include "spatial_audio.h"
static SDL_SpinLock sLock;
static struct {int active,listener,secondListener,count,alive[10];float pos[40];Uint64 at;} sWorld;
void PortAudio_World(int active,int listener,int secondListener,int count,const float *positions,const int *alive){
    SDL_LockSpinlock(&sLock);
    sWorld.active=active&&count>0&&count<=10&&listener>=0&&listener<count;
    sWorld.listener=listener;sWorld.count=sWorld.active?count:0;sWorld.at=SDL_GetTicks();
    sWorld.secondListener=secondListener>=0&&secondListener<sWorld.count?secondListener:-1;
    if(sWorld.active){memcpy(sWorld.pos,positions,count*4*sizeof(float));memcpy(sWorld.alive,alive,count*sizeof(int));}
    SDL_UnlockSpinlock(&sLock);
}
static float distanceSquared(const float *source,const float *listener){
    const float x=source[0]-listener[0],y=source[1]-listener[1],z=source[2]-listener[2];
    return x*x+y*y+z*z;
}
float PortAudio_Gain(int actor,int voice){
    float gain=1;SDL_LockSpinlock(&sLock);
    if(sWorld.active&&actor>=0&&actor<sWorld.count){
        const float *source=sWorld.pos+actor*4;
        float distance=distanceSquared(source,sWorld.pos+sWorld.listener*4);
        if(sWorld.secondListener>=0)
            distance=fminf(distance,distanceSquared(source,sWorld.pos+sWorld.secondListener*4));
        gain=SDL_GetTicks()-sWorld.at>1500?0:Bt4_AudioFalloff(sqrtf(distance),voice?8:10,voice?120:160);
    }
    SDL_UnlockSpinlock(&sLock);return gain;
}
int PortAudio_Alive(int actor){
    int alive=1;SDL_LockSpinlock(&sLock);
    if(sWorld.active&&actor>=0&&actor<sWorld.count)alive=sWorld.alive[actor];
    SDL_UnlockSpinlock(&sLock);return alive;
}
