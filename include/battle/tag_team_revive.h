#ifndef BATTLE_TAG_TEAM_REVIVE_H
#define BATTLE_TAG_TEAM_REVIVE_H
/* Local Dev rules. Network simulation must negotiate these before enabling them. */
void TagRevive_Reset(void);
void TagRevive_PrepareInput(void);
void TagRevive_Update(void);
s32 TagRevive_Busy(s32 actor);
s32 TagRevive_Hud(s32 actor, s32 *progress, s32 *total, s32 *helpers);
s32 TagRevive_Opacity(void);
f32 TagRevive_Radius(void);
#endif
