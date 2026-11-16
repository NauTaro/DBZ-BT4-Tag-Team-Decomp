/* GPL-3.0-or-later. Ally anchors carried by the same render list as the scene.
 * Lua widgets may be built a blank earlier; their final position follows the
 * picture being presented, including when rendering and simulation overlap. */
#ifndef TAG_HUD_ANCHOR_H
#define TAG_HUD_ANCHOR_H
typedef struct ThaState { unsigned valid[2];unsigned x[2][10],y[2][10]; } ThaState;
static int ThaMarker(ThaState *state,unsigned marker){
    unsigned actor,pane;
    if(marker==0x60000000u){state->valid[0]=state->valid[1]=0;return 1;}
    if((marker&0xf0000000u)!=0x50000000u)return 0;
    actor=(marker>>24)&15;pane=(marker>>23)&1;
    if(actor>=10)return 1;
    state->x[pane][actor]=(marker>>11)&4095;state->y[pane][actor]=marker&2047;
    state->valid[pane]|=1u<<actor;
    return 1;
}
static int ThaGet(const ThaState *state,int pane,int actor,float *x,float *y){
    if(pane<0||pane>1||actor<0||actor>=10||!(state->valid[pane]&(1u<<actor)))return 0;
    *x=state->x[pane][actor]/4095.0f;*y=state->y[pane][actor]/2047.0f;
    return 1;
}
#endif
