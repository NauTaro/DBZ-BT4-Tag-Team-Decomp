#ifndef TAG_TEAM_ASSIST_H
#define TAG_TEAM_ASSIST_H
#include "types.h"
void TagAssist_Reset(void);
void TagAssist_ReadPads(void);
void TagAssist_PrepareInput(void);
void TagAssist_Update(void);
s32 TagAssist_TakeTechnique(s32 actor);
/* 0 hidden, 1 available, 2 outgoing, 3 incoming, 4 coordinated attack active. */
s32 TagAssist_Hud(s32 actor,s32 *partner,s32 *ticks);
s32 TagAssist_CameraPartner(s32 actor);
s32 TagAssist_Busy(s32 actor);
/* Dev-local launch relay. Only confirmed native melee impacts advance it. */
void TagRelay_Hit(s32 attacker,s32 defender,s32 reaction,s32 previousAction);
s32 TagRelay_Reaction(s32 attacker,s32 defender,s32 reaction,s32 previousAction);
s32 TagRelay_TakeAction(s32 actor);
s32 TagRelay_Hud(s32 actor,s32 *partner,s32 *links,s32 *ticks);
s32 TagRelay_Engaged(s32 actor);
f32 TagRelay_RushScale(s32 actor);
f32 TagRelay_LaunchYaw(s32 attacker,s32 defender,f32 nativeYaw);
s32 TagRelay_IgnoreBodyStop(s32 actor);
#endif
