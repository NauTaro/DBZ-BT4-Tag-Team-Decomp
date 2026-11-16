#ifndef BATTLE_TRAINING_H
#define BATTLE_TRAINING_H
#include "types.h"
#include "battle/hud_0.h"
s32 Training_Active(void);
void Training_Init(void);
void Training_Tick(void);
void Training_Restart(void);
BtlMenu *Training_Menu(void);
u16 *Training_Text(s32 id);
s32 Training_ControlledActor(void);
void Training_FilterInput(s32 actor,u32 *buttons,u8 stick[2]);
#endif
