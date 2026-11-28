#ifndef BT4_REPLAY_VIDEO_H
#define BT4_REPLAY_VIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

int Port_ReplayVideoAvailable(void);
int Port_ReplayVideoActive(void);
int Port_ReplayVideoStart(void);
void Port_ReplayVideoStop(void);
int Port_ReplayVideoStatus(void);
int Port_ReplayVideoDue(void);
void Port_ReplayVideoFrame(const void *pixels, int width, int height, int pitch, int bgra, int flipped);
void Port_ReplayVideoAudio(const float *samples, int bytes, int rate, int channels);

#ifdef __cplusplus
}
#endif
#endif
