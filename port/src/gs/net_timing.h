#ifndef BT4_NET_TIMING_H
#define BT4_NET_TIMING_H

#include <stdint.h>

enum { NET_TIMING_PROBES = 16, NET_TIMING_RTT_LIMIT = 5000, NET_TIMING_DELAY_LIMIT = 16 };

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

static NetTimingSettings NetTiming_Choose(NetTimingQuality quality, int speed, int relayed) {
    const int tick_us = 1668300 / speed;
    const int median = quality.median < 0 ? (relayed ? 300 : 200) : quality.median;
    const int p90 = quality.p90 < 0 ? median : quality.p90;
    /* Each input arrives after about half an RTT plus polling. Keep a spare
       frame at the tail and more on a relay or when multiple probes miss. */
    const int typical = NetTiming_RttFrames(median, tick_us) + 1;
    const int reserve = 1 + relayed + (quality.missed >= 2) + (quality.missed >= 5);
    const int budget = NetTiming_RttFrames(p90, tick_us) + 1 + reserve;
    int delay = typical - 6;
    if (delay < budget - 8) delay = budget - 8;
    if (delay < 1) delay = 1;
    if (delay > NET_TIMING_DELAY_LIMIT) delay = NET_TIMING_DELAY_LIMIT;
    int rollback = (budget - delay + 1) & ~1;
    if (rollback < 2) rollback = 2;
    if (rollback > 8) rollback = 8;
    return (NetTimingSettings){delay, rollback};
}

#endif
