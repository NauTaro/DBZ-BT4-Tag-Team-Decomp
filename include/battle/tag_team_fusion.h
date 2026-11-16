#ifndef TAG_TEAM_FUSION_H
#define TAG_TEAM_FUSION_H
#include "types.h"
void TagFusion_Reset(void);
void TagFusion_ReadPads(void);
void TagFusion_Update(void);
s32 TagFusion_Local(void);
s32 TagFusion_Request(s32 actor,s32 index);
s32 TagFusion_Busy(s32 actor);
s32 TagFusion_Absent(s32 actor);
s32 TagFusion_ViewActor(s32 actor);
s32 TagFusion_Separating(s32 actor);
void TagFusion_Action(void *fighter,s32 phase);
s32 TagFusion_OnModelLoaded(void *fighter);
void TagFusion_AfterModelLoaded(void *fighter);
s32 TagFusion_Hud(s32 actor,s32 *other,s32 *ticks,s32 *total);
s32 TagFusion_Controller(s32 actor);
s32 TagFusion_CPUWaiting(s32 actor);
s32 TagFusion_ClashExtra(s32 actor,s32 input);
s32 TagFusion_ClashChoice(s32 actor,s32 pick,s32 previous);
s32 TagFusion_InputActor(s32 actor);
s32 TagFusion_Pad(s32 actor,s32 fallback);
void TagFusion_BeforeModelLoaded(void *fighter);
#endif
