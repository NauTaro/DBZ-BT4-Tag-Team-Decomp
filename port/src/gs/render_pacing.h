#ifndef BT4_RENDER_PACING_H
#define BT4_RENDER_PACING_H
#include <stdint.h>

/* Additional presentation may consume spare time, never another game tick.
 * No frequency, monitor refresh or hard-coded frame count enters this test.
 * Subtraction keeps the deadline safe when a large monotonic clock is used.
 */
static inline int GsPresent_UnlimitedDone(uint64_t now, uint64_t began,
                                        uint64_t tick, uint64_t cost, int waiting) {
    if (waiting || !tick || cost >= tick) return 1;
    return now >= began && now - began >= tick - cost;
}

static inline float GsPresent_UnlimitedTime(uint64_t now,uint64_t began,uint64_t tick) {
    if (!tick || (now >= began && now - began >= tick)) return 1.0f;
    if (now <= began) return 0.0f;
    return (float)(now - began) / (float)tick;
}
#endif
