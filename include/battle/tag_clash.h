#ifndef BATTLE_TAG_CLASH_H
#define BATTLE_TAG_CLASH_H
#include "types.h"

/* Shared by the partial fighter/roster views of action, placement and flags. */
s32 TagClash_BeginVanish(s32 a, s32 b);
void *TagClash_GetExchange(s32 actor);
s32 TagClash_ExchangeSide(s32 actor);

#endif
