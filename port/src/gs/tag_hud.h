/* Separate, bounded presentation snapshot. The existing FpState ABI is unchanged. */
#ifndef PORT_TAG_HUD_H
#define PORT_TAG_HUD_H
#include "fight_panels.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct TagHudActor {
    FpMember portrait;
    int found,team,cpu,ki,kiMax,blast,blastMax,maxPower,poweredUp;
} TagHudActor;
typedef struct TagHudFloating {
    int actor,x,y; /* actor slot; x/y normalised to the full picture, 0..100000 */
} TagHudFloating;
typedef struct TagHudState {
    int active,count,view,target,split;
    TagHudActor actor[10];
    int views[2], spectating[2];
    int floatingCount[2];
    TagHudFloating floating[2][4];
} TagHudState;
int PortPanels_TagRead(TagHudState *out);
void Port_TagAllyHudReset(void);
void Port_TagAllyHudFeed(int pane,int viewer,int actor,int x,int y);
#ifdef __cplusplus
}
#endif
#endif
