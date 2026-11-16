/* Dev Tag Team background. One streamed all-intra movie, two reusable CPU
 * pictures and one GPU texture. No external player, subprocess, audio, frame
 * atlas, gameplay update or replacement of disc textures is involved.
 * The existing movie decoder is compiled here with the native renderer's
 * integer arithmetic, isolated from the emulated game's FMV decoder/state. */
#include <SDL3/SDL.h>
#include "tag_menu_movie.h"
#define Mpeg2 TagMovieDecoder
#define Mpeg2_Open TagMovieDecoder_Open
#define Mpeg2_Close TagMovieDecoder_Close
#define Mpeg2_Next TagMovieDecoder_Next
#define Mpeg2_Width TagMovieDecoder_Width
#define Mpeg2_Height TagMovieDecoder_Height
#include "../plat_mpeg2.c"

enum { MOVIE_W = 1280, MOVIE_H = 720, MOVIE_BYTES = MOVIE_W * MOVIE_H * 4 };
static SDL_AtomicInt sMovieRestart;
void Port_TagMenuMovieBegin(void) { SDL_SetAtomicInt(&sMovieRestart,1); }
static struct {
    SDL_Mutex *lock;
    SDL_Condition *wake;
    SDL_Thread *thread;
    uint8_t *ready;
    uint32_t serial;
    uint64_t touched;
    int stop, finished, failed;
    char intro[1200], loop[1200];
} sMovie;

/* Check dimensions before writing RGBA, including a locally damaged asset.
 * next_planes owns at most one compressed picture and one YUV picture. */
static int movie_decode(TagMovieDecoder *m, uint8_t *rgba) {
    int x, y, stride;
    if (!next_planes(m) || m->width != MOVIE_W || m->height != MOVIE_H || !m->y) return 0;
    stride = m->mbw * 16;
    for (y = 0; y < MOVIE_H; ++y) {
        const uint8_t *py=m->y+y*stride,*pb=m->cb+(y>>1)*(stride>>1),*pr=m->cr+(y>>1)*(stride>>1);
        uint8_t *o=rgba+(size_t)y*MOVIE_W*4;
        for (x = 0; x < MOVIE_W; ++x) {
            int c=76309*(py[x]-16)+32768,d=pb[x>>1]-128,e=pr[x>>1]-128;
            int r=(c+104597*e)>>16,g=(c-25675*d-53279*e)>>16,b=(c+132201*d)>>16;
            o[x*4]=(uint8_t)(r<0?0:r>255?255:r);
            o[x*4+1]=(uint8_t)(g<0?0:g>255?255:g);
            o[x*4+2]=(uint8_t)(b<0?0:b>255?255:b);
            o[x*4+3]=128; /* native GS opaque alpha */
        }
    }
    return 1;
}

static int movie_worker(void *unused) {
    TagMovieDecoder *m = TagMovieDecoder_Open(sMovie.intro);
    uint8_t *decode = malloc(MOVIE_BYTES), *other = malloc(MOVIE_BYTES);
    uint64_t origin=0, frames=0, decodeNs=0, worstNs=0;
    unsigned loops=0;
    int failed=1;
    (void)unused;
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    if (!m || !decode || !other) goto done;
    SDL_LockMutex(sMovie.lock);
    sMovie.ready=other; other=NULL;
    SDL_UnlockMutex(sMovie.lock);
    for (;;) {
        uint64_t now, due, before, elapsed;
        SDL_LockMutex(sMovie.lock);
        now=SDL_GetTicksNS();
        if (sMovie.stop || now-sMovie.touched>500000000ull) {
            failed=0;
            SDL_UnlockMutex(sMovie.lock); break;
        }
        due=origin+frames*1001000000000ull/24000;
        if (origin && now<due) {
            int wait=(int)((due-now+999999ull)/1000000ull);
            SDL_WaitConditionTimeout(sMovie.wake,sMovie.lock,wait);
            SDL_UnlockMutex(sMovie.lock); continue;
        }
        SDL_UnlockMutex(sMovie.lock);
        before=SDL_GetTicksNS();
        if (!movie_decode(m,decode)) {
            TagMovieDecoder_Close(m);
            m=TagMovieDecoder_Open(sMovie.loop);
            if (!m || !movie_decode(m,decode)) break;
            ++loops;
        }
        elapsed=SDL_GetTicksNS()-before;decodeNs+=elapsed;
        if(elapsed>worstNs)worstNs=elapsed;
        SDL_LockMutex(sMovie.lock);
        other=sMovie.ready;sMovie.ready=decode;decode=other;other=NULL;
        ++sMovie.serial;
        SDL_UnlockMutex(sMovie.lock);
        now=SDL_GetTicksNS();
        if(!origin)origin=now;
        ++frames;
        /* A slow/minimized PC retains bounded memory and does not build an
         * unbounded decode queue trying to recover its wall-clock position. */
        if(now>origin+frames*1001000000000ull/24000+85000000ull)
            origin=now-frames*1001000000000ull/24000;
    }
done:
    TagMovieDecoder_Close(m); free(decode);free(other);
    SDL_LockMutex(sMovie.lock);
    free(sMovie.ready);sMovie.ready=NULL;sMovie.serial=0;sMovie.finished=1;sMovie.failed=failed;
    SDL_UnlockMutex(sMovie.lock);
    if(getenv("BT4_TAG_MOVIE_AUDIT"))
        fprintf(stderr,"tag menu movie: frames=%llu loops=%u decode-average-us=%llu decode-max-us=%llu cpu-picture-bytes=%d released=1\n",
                (unsigned long long)frames,loops,(unsigned long long)(frames?decodeNs/frames/1000:0),
                (unsigned long long)(worstNs/1000),MOVIE_BYTES*2);
    return 0;
}

void TagMenuMovie_Maintain(void) {
    int finished;
    if(!sMovie.thread)return;
    SDL_LockMutex(sMovie.lock);finished=sMovie.finished;SDL_UnlockMutex(sMovie.lock);
    if(finished){SDL_WaitThread(sMovie.thread,NULL);sMovie.thread=NULL;}
}

unsigned char *TagMenuMovie_Pixels(uint32_t *width,uint32_t *height,uint32_t *serial) {
    uint8_t *pixels=NULL;
    const char *base=SDL_GetBasePath();
    SDL_PathInfo info;
    if(SDL_CompareAndSwapAtomicInt(&sMovieRestart,1,0)){
        TagMenuMovie_Stop();*serial=0;
    }
    TagMenuMovie_Maintain();
    if(!sMovie.lock) {
        sMovie.lock=SDL_CreateMutex();sMovie.wake=SDL_CreateCondition();
    }
    if(!sMovie.lock||!sMovie.wake)return NULL;
    SDL_LockMutex(sMovie.lock);
    if(sMovie.failed && SDL_GetTicksNS()-sMovie.touched<500000000ull) {
        sMovie.touched=SDL_GetTicksNS();SDL_UnlockMutex(sMovie.lock);return NULL;
    }
    sMovie.touched=SDL_GetTicksNS();
    if(!sMovie.thread) {
        SDL_snprintf(sMovie.intro,sizeof(sMovie.intro),"%stbo/ui/tag_ui_intro.pss",base?base:"");
        SDL_snprintf(sMovie.loop,sizeof(sMovie.loop),"%stbo/ui/tag_ui_loop.pss",base?base:"");
        /* Missing assets fall back to the static artwork, without an open/file
         * failure every single frame. Re-entering the menu may retry. */
        if(!SDL_GetPathInfo(sMovie.intro,&info)||!SDL_GetPathInfo(sMovie.loop,&info)){
            sMovie.failed=1;
            SDL_UnlockMutex(sMovie.lock);return NULL;
        }
        sMovie.stop=0;sMovie.finished=0;sMovie.failed=0;sMovie.serial=0;*serial=0;
        sMovie.thread=SDL_CreateThread(movie_worker,"Tag menu movie",NULL);
    }
    if(sMovie.serial && sMovie.serial!=*serial && sMovie.ready) {
        pixels=malloc(MOVIE_BYTES);
        if(pixels){memcpy(pixels,sMovie.ready,MOVIE_BYTES);*serial=sMovie.serial;}
    }
    SDL_UnlockMutex(sMovie.lock);
    *width=MOVIE_W;*height=MOVIE_H;
    return pixels;
}

void TagMenuMovie_Stop(void) {
    if(!sMovie.thread)return;
    SDL_LockMutex(sMovie.lock);sMovie.stop=1;SDL_SignalCondition(sMovie.wake);SDL_UnlockMutex(sMovie.lock);
    SDL_WaitThread(sMovie.thread,NULL);sMovie.thread=NULL;
}
