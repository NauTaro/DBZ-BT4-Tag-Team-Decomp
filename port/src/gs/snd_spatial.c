/* Presentation-only battle snapshot; no audio callback reads the game heap. */
#include <SDL3/SDL.h>
#include <string.h>
#include "battle/emotes.h"
#include "spatial_audio.h"
static SDL_SpinLock sLock;
static struct {int active,listener,secondListener,count,alive[10];float pos[40];Uint64 at;} sWorld;
void PortAudio_World(int active,int listener,int secondListener,int count,const float *positions,const int *alive){
    SDL_LockSpinlock(&sLock);
    sWorld.active=active&&count>0&&count<=10&&listener>=0&&listener<count;
    sWorld.listener=listener;sWorld.count=sWorld.active?count:0;sWorld.at=SDL_GetTicks();
    sWorld.secondListener=secondListener>=0&&secondListener<sWorld.count?secondListener:-1;
    if(sWorld.active){memcpy(sWorld.pos,positions,count*4*sizeof(float));memcpy(sWorld.alive,alive,count*sizeof(int));}
    SDL_UnlockSpinlock(&sLock);
}
static float distanceSquared(const float *source,const float *listener){
    const float x=source[0]-listener[0],y=source[1]-listener[1],z=source[2]-listener[2];
    return x*x+y*y+z*z;
}
float PortAudio_Gain(int actor,int voice){
    float gain=1;SDL_LockSpinlock(&sLock);
    if(sWorld.active&&actor>=0&&actor<sWorld.count){
        const float *source=sWorld.pos+actor*4;
        float distance=distanceSquared(source,sWorld.pos+sWorld.listener*4);
        if(sWorld.secondListener>=0)
            distance=fminf(distance,distanceSquared(source,sWorld.pos+sWorld.secondListener*4));
        gain=SDL_GetTicks()-sWorld.at>1500?0:Bt4_AudioFalloff(sqrtf(distance),voice?8:10,voice?120:160);
    }
    SDL_UnlockSpinlock(&sLock);return gain;
}
int PortAudio_Alive(int actor){
    int alive=1;SDL_LockSpinlock(&sLock);
    if(sWorld.active&&actor>=0&&actor<sWorld.count)alive=sWorld.alive[actor];
    SDL_UnlockSpinlock(&sLock);return alive;
}

enum { EMOTE_AUDIO_ACTORS = 10, EMOTE_AUDIO_RATE = 32000 };

typedef struct {
    Sint16 *pcm;
    int count;
    int attempted;
} EmoteClip;

static EmoteClip sEmoteClips[BTL_EMOTE_COUNT + 1];

typedef struct {
    const EmoteClip *clip;
    int playing;
    int sample;
    float frame;
    float gain;
    float target;
} EmoteVoice;

static struct {
    SDL_AudioStream *stream;
    Uint64 at;
    EmoteVoice voices[EMOTE_AUDIO_ACTORS];
} sEmoteAudio;

/* SDL holds the stream lock for the callback. The game publishes a copy of
 * the pose and mix gains, so audio playback is outside the rollback state. */
static void SDLCALL mixEmotes(void *userdata, SDL_AudioStream *stream, int additional, int total) {
    (void)userdata;
    (void)total;
    const int fresh = SDL_GetTicks() - sEmoteAudio.at < 400;
    const float gainStep = 1.f / (.025f * EMOTE_AUDIO_RATE);
    int remaining = (additional + sizeof(Sint16) - 1) / sizeof(Sint16);
    while (remaining > 0) {
        Sint16 buffer[512];
        const int capacity = (int)SDL_arraysize(buffer);
        const int length = remaining < capacity ? remaining : capacity;
        for (int i = 0; i < length; ++i) {
            float sample = 0.f;
            for (int actor = 0; actor < EMOTE_AUDIO_ACTORS; ++actor) {
                EmoteVoice *voice = &sEmoteAudio.voices[actor];
                const int playing = fresh && voice->playing;
                const float target = playing ? voice->target : 0.f;
                if (!playing && voice->gain <= 0.f) continue;
                voice->gain += SDL_clamp(target - voice->gain, -gainStep, gainStep);
                sample += voice->clip->pcm[voice->sample] * voice->gain;
                if (++voice->sample == voice->clip->count) voice->sample = 0;
            }
            buffer[i] = (Sint16)SDL_clamp(sample, -32768.f, 32767.f);
        }
        if (!SDL_PutAudioStreamData(stream, buffer, length * sizeof(*buffer))) break;
        remaining -= length;
    }
}

static const EmoteClip *loadEmoteClip(int id) {
    extern int PortMa_DecodeEmote(const char *, unsigned, Sint16 **);
    if (id <= 0 || id > BTL_EMOTE_COUNT) return NULL;
    EmoteClip *clip = &sEmoteClips[id];
    if (clip->attempted) return clip->count ? clip : NULL;
    clip->attempted = 1;
    const char *base = SDL_GetBasePath();
    char *path = NULL;
    if (!base || SDL_asprintf(&path, "%stbo/emote-audio/%02d.mp3", base, id) < 0) return NULL;
    clip->count = PortMa_DecodeEmote(path, EMOTE_AUDIO_RATE, &clip->pcm);
    SDL_free(path);
    if (!clip->count && id == BTL_EMOTE_CELEBRATE_ME &&
        SDL_asprintf(&path, "%stbo/emote-audio/celebrate-me.wav", base) >= 0) {
        clip->count = PortMa_DecodeEmote(path, EMOTE_AUDIO_RATE, &clip->pcm);
        SDL_free(path);
    }
    return clip->count ? clip : NULL;
}

static void openEmoteAudio(SDL_AudioDeviceID device) {
    const SDL_AudioSpec spec = {SDL_AUDIO_S16, 1, EMOTE_AUDIO_RATE};
    SDL_AudioStream *stream = SDL_CreateAudioStream(&spec, NULL);
    if (stream && SDL_SetAudioStreamGetCallback(stream, mixEmotes, NULL) &&
        SDL_BindAudioStream(device, stream)) {
        sEmoteAudio.stream = stream;
    } else {
        SDL_DestroyAudioStream(stream);
    }
}

void PortAudio_Emotes(int count, const int *ids, const float *frames, const float *positions,
                      int listener, int secondListener) {
    extern SDL_AudioDeviceID Port_AudioDevice(void);
    extern float PortVoice_ProximityGain(float);
    extern void Port_EmotePauseMusic(int);
    extern int gPortSePercent;

    const EmoteClip *clips[EMOTE_AUDIO_ACTORS] = {0};
    const SDL_AudioDeviceID device = Port_AudioDevice();
    if (!device || count <= 0 || count > EMOTE_AUDIO_ACTORS || listener < 0 || listener >= count) count = 0;
    for (int actor = 0; actor < count; ++actor) {
        if (frames[actor] >= 0.f) clips[actor] = loadEmoteClip(ids[actor]);
        if (clips[actor] && !sEmoteAudio.stream) {
            openEmoteAudio(device);
        }
    }
    if (!sEmoteAudio.stream || !SDL_LockAudioStream(sEmoteAudio.stream)) return;
    const Uint64 now = SDL_GetTicks();
    const int stale = now - sEmoteAudio.at >= 400;
    sEmoteAudio.at = now;
    int audible = 0;
    for (int actor = 0; actor < EMOTE_AUDIO_ACTORS; ++actor) {
        EmoteVoice *voice = &sEmoteAudio.voices[actor];
        const EmoteClip *clip = clips[actor];
        if (!clip) {
            voice->playing = 0;
            continue;
        }
        const float frame = frames[actor];
        if (stale || !voice->playing || voice->clip != clip || (frame <= 2.f && voice->frame > 6.f)) {
            voice->clip = clip;
            voice->sample = (int)fmodf(frame * (EMOTE_AUDIO_RATE / 60.f), clip->count);
            voice->gain = 0.f;
        }
        const float *source = positions + actor * 4;
        float distance = distanceSquared(source, positions + listener * 4);
        if (secondListener >= 0 && secondListener < count)
            distance = fminf(distance, distanceSquared(source, positions + secondListener * 4));
        voice->frame = frame;
        voice->playing = 1;
        voice->target = PortVoice_ProximityGain(sqrtf(distance)) * (SDL_clamp(gPortSePercent, 0, 100) / 100.f);
        audible |= voice->target > 0.f;
    }
    SDL_UnlockAudioStream(sEmoteAudio.stream);
    Port_EmotePauseMusic(audible);
}
