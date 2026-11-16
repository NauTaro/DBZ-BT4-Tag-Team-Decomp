#ifndef BT4_RANKED_BALL_ART_H
#define BT4_RANKED_BALL_ART_H
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* Presentation-only procedural artwork. 128px icons, 4x4 coverage samples per
 * pixel; cached by the renderer. No fight state, files or texture downloads. */
static float Rb_Clamp(float x){return x<0?0:x>1?1:x;}
static int Rb_Star(float x,float y){
    static const float vx[10]={0,.264503f,.951057f,.427975f,.587785f,0,-.587785f,-.427975f,-.951057f,-.264503f};
    static const float vy[10]={-1,-.364058f,-.309017f,.139058f,.809017f,.45f,.809017f,.139058f,-.309017f,-.364058f};
    int inside=0;
    if(x<-1||x>1||y<-1||y>1)return 0;
    for(int i=0,j=9;i<10;j=i++)if((vy[i]>y)!=(vy[j]>y)&&x<(vx[j]-vx[i])*(y-vy[i])/(vy[j]-vy[i])+vx[i])inside=!inside;
    return inside;
}
static uint32_t *RankedBall_Pixels(int stars){
    static const float marks[7][7][2]={
        {{0,0}},{{-.25f,-.23f},{.25f,.23f}},{{0,-.32f},{-.29f,.22f},{.29f,.22f}},
        {{-.27f,-.27f},{.27f,-.27f},{-.27f,.27f},{.27f,.27f}},
        {{-.32f,-.32f},{.32f,-.32f},{0,0},{-.32f,.32f},{.32f,.32f}},
        {{-.28f,-.4f},{.28f,-.4f},{-.28f,0},{.28f,0},{-.28f,.4f},{.28f,.4f}},
        {{0,-.45f},{-.39f,-.23f},{.39f,-.23f},{0,0},{-.39f,.23f},{.39f,.23f},{0,.45f}}
    };
    const int size=128,samples=4;
    uint32_t *pixels=(uint32_t*)malloc(size*size*sizeof(uint32_t));
    if(!pixels||stars<0||stars>7){free(pixels);return NULL;}
    for(int py=0;py<size;py++)for(int px=0;px<size;px++){
        float color[3]={0,0,0},alpha=0;
        for(int sy=0;sy<samples;sy++)for(int sx=0;sx<samples;sx++){
            float x=((px+(sx+.5f)/samples)/size*2-1)/.94f;
            float y=((py+(sy+.5f)/samples)/size*2-1)/.94f;
            float distanceSquared=x*x+y*y;
            if(distanceSquared>=1)continue;
            // Fade over the outer tenth of the radius so the circle still has
            // a smooth silhouette when drawn on the lowest-resolution target.
            float edge=Rb_Clamp((1-sqrtf(distanceSquared))/.10f);
            float coverage=edge*edge*(3-2*edge);
            float z=sqrtf(1-distanceSquared),light=Rb_Clamp(-.34f*x-.46f*y+.82f*z);
            float rim=powf(1-z,3),shine=expf(-((x+.33f)*(x+.33f)/.026f+(y+.39f)*(y+.39f)/.015f));
            float r=stars?(.67f+.33f*light-.19f*rim):(.13f+.11f*light);
            float g=stars?(.20f+.53f*light-.14f*rim):(.26f+.15f*light);
            float b=stars?(.018f+.085f*light):(.29f+.15f*light);
            float starRadius=stars==1?.25f:.18f;
            for(int i=0;i<stars;i++)if(Rb_Star((x-marks[stars-1][i][0])/starRadius,(y-marks[stars-1][i][1])/starRadius)){
                r=.62f+.12f*light;g=.035f+.025f*light;b=.018f;break;
            }
            // A restrained warm reflection, rather than a white border.
            r=r*(1-.72f*shine)+shine*.72f;
            g=g*(1-.72f*shine)+shine*.69f;
            b=b*(1-.72f*shine)+shine*.40f;
            color[0]+=Rb_Clamp(r)*coverage;
            color[1]+=Rb_Clamp(g)*coverage;
            color[2]+=Rb_Clamp(b)*coverage;
            alpha+=coverage;
        }
        if(alpha){
            unsigned r=(unsigned)(color[0]/alpha*255+.5f),g=(unsigned)(color[1]/alpha*255+.5f),b=(unsigned)(color[2]/alpha*255+.5f);
            unsigned a=(unsigned)(alpha/(samples*samples)*128+.5f);
            pixels[py*size+px]=r|(g<<8)|(b<<16)|(a<<24);
        }else pixels[py*size+px]=0;
    }
    return pixels;
}
#endif
