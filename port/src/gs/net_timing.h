#ifndef BT4_NET_TIMING_H
#define BT4_NET_TIMING_H

#include <stdint.h>

enum { NET_TIMING_PROBES = 16, NET_TIMING_RTT_LIMIT = 5000, NET_TIMING_DELAY_LIMIT = 30 };

typedef struct NetTimingProbes {
    uint32_t sent_at[NET_TIMING_PROBES];
    int rtt[NET_TIMING_PROBES]; /* -1 until the probe is answered */
    int sent, received;
} NetTimingProbes;

typedef struct NetTimingQuality {
    int median, p90, missed, sent;
} NetTimingQuality;

typedef struct NetTimingSettings {
    int delay, rollback;
} NetTimingSettings;

/* Called once when a probe is sent. The timestamp is echoed by old clients too. */
static void NetTiming_Send(NetTimingProbes *probes, uint32_t timestamp) {
    if (probes->sent == NET_TIMING_PROBES) return;
    const int i = probes->sent++;
    probes->sent_at[i] = timestamp;
    probes->rtt[i] = -1;
}

static int NetTiming_Receive(NetTimingProbes *probes, uint32_t timestamp, uint32_t now) {
    const uint32_t rtt = now - timestamp;
    if (rtt >= NET_TIMING_RTT_LIMIT) return -1;
    for (int i = 0; i < probes->sent; ++i) {
        if (probes->sent_at[i] != timestamp || probes->rtt[i] >= 0) continue;
        probes->rtt[i] = (int)rtt;
        ++probes->received;
        return (int)rtt;
    }
    return -1;
}

static NetTimingQuality NetTiming_Summarize(const NetTimingProbes *probes) {
    NetTimingQuality quality = {-1, -1, probes->sent - probes->received, probes->sent};
    int samples[NET_TIMING_PROBES], count = 0;
    for (int i = 0; i < probes->sent; ++i) {
        const int rtt = probes->rtt[i];
        if (rtt < 0) continue;
        int j = count++;
        while (j > 0 && samples[j - 1] > rtt) {
            samples[j] = samples[j - 1];
            --j;
        }
        samples[j] = rtt;
    }
    if (count > 0) {
        quality.median = (samples[(count - 1) / 2] + samples[count / 2]) / 2;
        quality.p90 = samples[(count * 9 + 9) / 10 - 1];
    }
    return quality;
}

static int NetTiming_RttFrames(int rtt, int tick_us) {
    return (rtt * 500 + tick_us - 1) / tick_us;
}

typedef enum NetTimingProfile {
    NET_TIMING_ROLLBACK,
    NET_TIMING_AUTO,
    NET_TIMING_DELAY
} NetTimingProfile;

static int NetTiming_DelayMs(int rtt, NetTimingProfile profile) {
    static const struct {
        int maxPing;
        uint8_t delay[3];
    } table[] = {
        { 16, {  0,   0,   8}}, { 33, {  0,   8,  17}},
        { 50, {  8,  17,  25}}, { 66, {  8,  25,  33}},
        { 83, { 17,  33,  42}}, {100, { 17,  42,  50}},
        {116, { 25,  50,  58}}, {133, { 25,  58,  67}},
        {150, { 33,  67,  75}}, {166, { 33,  75,  83}},
        {183, { 42,  83, 100}}, {200, { 50, 100, 117}},
        {216, { 58, 108, 125}}, {233, { 67, 125, 142}},
        {250, { 75, 142, 158}}, {266, { 83, 150, 175}},
        {283, { 92, 167, 183}}, {300, {100, 175, 200}},
        {316, {108, 183, 208}}, {333, {117, 192, 225}}
    };
    for (unsigned i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (rtt <= table[i].maxPing) return table[i].delay[profile];
    }
    static const uint8_t highest[] = {125, 200, 233};
    return highest[profile];
}

/* The table is in milliseconds. Both players run the same whole game blanks,
   at the host's selected speed. Keep a single blank for any nonzero delay. */
static int NetTiming_DelayFrames(int rtt, int speed, NetTimingProfile profile) {
    const int ms = NetTiming_DelayMs(rtt, profile);
    const int tick_us = 1668300 / speed;
    const int frames = (ms * 1000 + tick_us / 2) / tick_us;
    if (frames > NET_TIMING_DELAY_LIMIT) return NET_TIMING_DELAY_LIMIT;
    return ms > 0 && frames == 0 ? 1 : frames;
}

static NetTimingSettings NetTiming_Choose(NetTimingQuality quality, int speed, int relayed) {
    const int tick_us = 1668300 / speed;
    const int median = quality.median < 0 ? (relayed ? 300 : 200) : quality.median;
    const int p90 = quality.p90 < 0 ? median : quality.p90;
    const int reserve = 1 + relayed + (quality.missed >= 2) + (quality.missed >= 5);
    const int budget = NetTiming_RttFrames(p90, tick_us) + 1 + reserve;
    const int delay = NetTiming_DelayFrames(median, speed, NET_TIMING_AUTO);
    int rollback = (budget - delay + 1) & ~1;
    if (rollback < 2) rollback = 2;
    if (rollback > 8) rollback = 8;
    return (NetTimingSettings){delay, rollback};
}

#endif
