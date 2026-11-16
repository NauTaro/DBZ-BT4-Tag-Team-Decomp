#ifndef BT4_RANKED_ROCK_H
#define BT4_RANKED_ROCK_H

#define BT4_ROCK_MAX_OBJECTS 64
#define BT4_ROCK_MAX_STAGES 256
#define BT4_ROCK_MARKER 0x52420000u

static inline unsigned RankedRock_Event(unsigned stage, unsigned object, unsigned side) {
    if (stage >= BT4_ROCK_MAX_STAGES || object >= BT4_ROCK_MAX_OBJECTS || side > 1) return 0;
    return 1 + (stage * BT4_ROCK_MAX_OBJECTS + object) * 2 + side;
}

#endif
