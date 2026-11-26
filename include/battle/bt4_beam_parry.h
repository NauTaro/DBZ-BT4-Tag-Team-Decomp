#ifndef BATTLE_BT4_BEAM_PARRY_H
#define BATTLE_BT4_BEAM_PARRY_H

#include "sys/math3d.h"

#define EFT_EMIT_MAX 40      /* emitters over all groups (handle slots per half) */

/* Particle objects of a set, one slot per emitter. */
typedef struct EftEmitHandles {
    /* 0x00 */ void *h[EFT_EMIT_MAX];
} EftEmitHandles; /* size 0xA0 */

#if defined(BT4_TAG_TEAM_EXPANDED) && defined(BT4_DEVELOPMENT_BUILD) && !defined(BT4_TAG_TEAM_PREVIEW)
#define BT4_DEV_BEAM_PARRY

struct EftEmitSet;
struct EftEmitState;

typedef struct Bt4ParriedBeam {
    EftEmitHandles redirected;
    Vec4 dir;
    Vec4 releasePoint;
    s32 frame;
} Bt4ParriedBeam;

s32 EftBeamParry_Update(Bt4ParriedBeam *parry, struct EftEmitSet *set, struct EftEmitState *original,
                        u32 tag, Vec4 *muzzle, Vec4 *incoming, Vec4 *head, f32 scale, f32 speed);
void EftBeamParry_Reset(Bt4ParriedBeam *parry, struct EftEmitSet *set);
#endif

/* Set only by the blast and sweep emitters when they build a beam capsule. */
#define BT4_HIT_BEAM_CAPSULE 0x2000

/* The beam task stores one defender (actor + 1).
 * BtlTask_Init clears these bits when the effect slot is reused. */
#define BT4_BEAM_PARRY_TAG_SHIFT 20
#define BT4_BEAM_PARRY_ACTOR_MASK (0xFu << BT4_BEAM_PARRY_TAG_SHIFT)
#define BT4_BEAM_PARRY_ACTORS 10
#define BT4_BEAM_PARRY_HAND 0x15
#define BT4_BEAM_PARRY_WINDUP 5
#define BT4_BEAM_PARRY_RELEASE 9
#define BT4_BEAM_PARRY_FRAMES 22
#define BT4_BEAM_PARRY_COST 300000

#endif
