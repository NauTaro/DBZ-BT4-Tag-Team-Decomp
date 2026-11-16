#ifndef BATTLE_TAG_HEALTH_RULE_H
#define BATTLE_TAG_HEALTH_RULE_H

#include "common.h"

typedef struct TagHealthRule {
    s32 multiplier;
    s32 maximum;
    s32 starting;
    s32 percent;
    s32 frozen;
} TagHealthRule;

#define TAG_HEALTH_LIMIT 9999999
void TagBattle_SetLocalHealth(const TagHealthRule rules[2][5]);
const TagHealthRule *TagBattle_HealthRule(s32 actor);

#endif
