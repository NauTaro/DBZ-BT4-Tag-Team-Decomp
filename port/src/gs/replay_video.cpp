#include "bt4_replay_video.h"
#include <SDL3/SDL.h>

#ifdef _WIN32
#define INITGUID
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace {
constexpr int fps = 30, audioRate = 48000;
constexpr Uint64 interval = SDL_NS_PER_SECOND / fps;
constexpr LONGLONG second = 10000000;
constexpr size_t queueLimit = 64 * 1024 * 1024;

struct Packet {
    std::vector<Uint8> data;
    Uint64 time;
    int width, height, rate, channels;
};

std::mutex mutex;
std::condition_variable ready;
std::deque<Packet> queue;
size_t queuedBytes;
bool closing;
SDL_Thread *worker;
std::atomic<int> status{0}; // 1 recording, 2 finalizing, 3 saved, -1 failed
std::atomic<Uint64> firstFrame{0};
std::atomic<Uint64> lastFrame{0};

template<class T> struct ComPtr {
    T *ptr = nullptr;
    ~ComPtr() { if (ptr) ptr->Release(); }
    T *operator->() const { return ptr; }
};

struct Encoder {
    HMODULE platform = LoadLibraryExW(L"mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE write = LoadLibraryExW(L"mfreadwrite.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE com = LoadLibraryExW(L"ole32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    decltype(&MFCreateMediaType) makeType = load<decltype(makeType)>(platform, "MFCreateMediaType");
    decltype(&MFCreateMemoryBuffer) makeBuffer = load<decltype(makeBuffer)>(platform, "MFCreateMemoryBuffer");
    decltype(&MFCreateSample) makeSample = load<decltype(makeSample)>(platform, "MFCreateSample");
    decltype(&MFCreateSinkWriterFromURL) makeWriter = load<decltype(makeWriter)>(write, "MFCreateSinkWriterFromURL");
    ComPtr<IMFSinkWriter> sink;
    SDL_AudioStream *audio = nullptr;
    int width = 0, height = 0, sourceRate = 0, sourceChannels = 0;
    DWORD videoTrack = 0, audioTrack = 0;
    LONGLONG audioFrames = 0;
    bool initialized = false, started = false;

    template<class T> static T load(HMODULE module, const char *name) {
        return reinterpret_cast<T>(module ? GetProcAddress(module, name) : nullptr);
    }
    ~Encoder() {
        if (sink.ptr) { sink.ptr->Release(); sink.ptr = nullptr; }
        if (audio) SDL_DestroyAudioStream(audio);
        if (started) load<decltype(&MFShutdown)>(platform, "MFShutdown")();
        if (initialized) load<decltype(&CoUninitialize)>(com, "CoUninitialize")();
        if (write) FreeLibrary(write);
        if (platform) FreeLibrary(platform);
        if (com) FreeLibrary(com);
    }
    bool videoType(IMFMediaType *type, const GUID &codec) const {
        const UINT64 size = (UINT64(width) << 32) | UINT32(height);
        return SUCCEEDED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
            SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, codec)) &&
            SUCCEEDED(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)) &&
            SUCCEEDED(type->SetUINT64(MF_MT_FRAME_SIZE, size)) &&
            SUCCEEDED(type->SetUINT64(MF_MT_FRAME_RATE, (UINT64(fps) << 32) | 1)) &&
            SUCCEEDED(type->SetUINT64(MF_MT_PIXEL_ASPECT_RATIO, (UINT64(1) << 32) | 1));
    }
    bool audioType(IMFMediaType *type, const GUID &codec) const {
        return SUCCEEDED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) &&
            SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, codec)) &&
            SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2)) &&
            SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, audioRate)) &&
            SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
    }
    bool open(const wchar_t *path, int w, int h) {
        auto init = load<decltype(&CoInitializeEx)>(com, "CoInitializeEx");
        auto start = load<decltype(&MFStartup)>(platform, "MFStartup");
        if (!init || !start || !makeType || !makeBuffer || !makeSample || !makeWriter) return false;
        initialized = SUCCEEDED(init(nullptr, COINIT_MULTITHREADED));
        started = initialized && SUCCEEDED(start(MF_VERSION, MFSTARTUP_FULL));
        if (!started || FAILED(makeWriter(path, nullptr, nullptr, &sink.ptr))) return false;

        width = w;
        height = h;
        ComPtr<IMFMediaType> h264, rgb, aac, pcm;
        return SUCCEEDED(makeType(&h264.ptr)) && SUCCEEDED(makeType(&rgb.ptr)) &&
            SUCCEEDED(makeType(&aac.ptr)) && SUCCEEDED(makeType(&pcm.ptr)) &&
            videoType(h264.ptr, MFVideoFormat_H264) && videoType(rgb.ptr, MFVideoFormat_RGB32) &&
            SUCCEEDED(h264->SetUINT32(MF_MT_AVG_BITRATE, 12000000)) &&
            SUCCEEDED(rgb->SetUINT32(MF_MT_DEFAULT_STRIDE, width * 4)) &&
            audioType(aac.ptr, MFAudioFormat_AAC) && audioType(pcm.ptr, MFAudioFormat_PCM) &&
            SUCCEEDED(aac->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000)) &&
            SUCCEEDED(pcm->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4)) &&
            SUCCEEDED(pcm->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, audioRate * 4)) &&
            SUCCEEDED(sink->AddStream(h264.ptr, &videoTrack)) &&
            SUCCEEDED(sink->AddStream(aac.ptr, &audioTrack)) &&
            SUCCEEDED(sink->SetInputMediaType(videoTrack, rgb.ptr, nullptr)) &&
            SUCCEEDED(sink->SetInputMediaType(audioTrack, pcm.ptr, nullptr)) &&
            SUCCEEDED(sink->BeginWriting());
    }
    bool sample(DWORD track, const void *data, DWORD bytes, LONGLONG time, LONGLONG duration) const {
        ComPtr<IMFMediaBuffer> buffer;
        ComPtr<IMFSample> sample;
        BYTE *target;
        if (FAILED(makeBuffer(bytes, &buffer.ptr)) || FAILED(buffer->Lock(&target, nullptr, nullptr))) return false;
        memcpy(target, data, bytes);
        buffer->Unlock();
        return SUCCEEDED(buffer->SetCurrentLength(bytes)) && SUCCEEDED(makeSample(&sample.ptr)) &&
            SUCCEEDED(sample->AddBuffer(buffer.ptr)) && SUCCEEDED(sample->SetSampleTime(time)) &&
            SUCCEEDED(sample->SetSampleDuration(duration)) && SUCCEEDED(sink->WriteSample(track, sample.ptr));
    }
    bool pcm(const void *data, int bytes) {
        const auto start = audioFrames * second / audioRate;
        audioFrames += bytes / 4;
        return sample(audioTrack, data, bytes, start, audioFrames * second / audioRate - start);
    }
    bool sound(const Packet &p) {
        if (!audio || sourceRate != p.rate || sourceChannels != p.channels) {
            if (audio) SDL_DestroyAudioStream(audio);
            const SDL_AudioSpec input{SDL_AUDIO_F32, p.channels, p.rate}, output{SDL_AUDIO_S16, 2, audioRate};
            audio = SDL_CreateAudioStream(&input, &output);
            sourceRate = p.rate;
            sourceChannels = p.channels;
        }
        if (!audio || !SDL_PutAudioStreamData(audio, p.data.data(), int(p.data.size()))) return false;
        Uint8 converted[16384];
        int bytes;
        while ((bytes = SDL_GetAudioStreamData(audio, converted, sizeof(converted))) > 0)
            if (!pcm(converted, bytes)) return false;
        return bytes == 0;
    }
};

void stopAtExit() {
    Port_ReplayVideoStop();
    if (worker) SDL_WaitThread(worker, nullptr);
}

int SDLCALL encode(void *) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t filename[128];
    swprintf(filename, sizeof(filename)/sizeof(*filename), L"replays\\DiosTube-%04u%02u%02u-%02u%02u%02u-%03u-%lu",
             now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
             (unsigned long)GetCurrentProcessId());
    const std::wstring temp = std::wstring(filename) + L".partial.mp4";
    const std::wstring final = std::wstring(filename) + L".mp4";
    bool ok = SDL_CreateDirectory("replays");
    std::vector<Uint8> previous;
    LONGLONG next = 0;
    {
        Encoder encoder;
        while (ok) {
            Packet p;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [] { return closing || !queue.empty(); });
                if (queue.empty()) break;
                p = std::move(queue.front());
                queue.pop_front();
                queuedBytes -= p.data.size();
            }
            if (!p.width) {
                if (encoder.sink.ptr) ok = encoder.sound(p);
                continue;
            }
            if (!encoder.sink.ptr) ok = encoder.open(temp.c_str(), p.width, p.height);
            const LONGLONG frame = (p.time * fps + SDL_NS_PER_SECOND / 2) / SDL_NS_PER_SECOND;
            while (ok && !previous.empty() && next < frame) {
                const auto time = next * second / fps;
                ++next;
                ok = encoder.sample(encoder.videoTrack, previous.data(), (DWORD)previous.size(), time, next * second / fps - time);
            }
            previous = std::move(p.data);
        }
        if (ok && !previous.empty()) {
            const auto time = next * second / fps;
            ok = encoder.sample(encoder.videoTrack, previous.data(), (DWORD)previous.size(), time, (next+1)*second/fps-time);
            const Uint8 silence[4096]{};
            const auto end = (next+1)*audioRate/fps;
            while (ok && encoder.audioFrames < end)
                ok = encoder.pcm(silence, int(SDL_min(end-encoder.audioFrames, LONGLONG(sizeof(silence)/4))*4));
            ok = ok && SUCCEEDED(encoder.sink->Finalize());
        } else ok = false;
    }
    ok = ok && MoveFileW(temp.c_str(), final.c_str());
    if (!ok) DeleteFileW(temp.c_str());
    else fprintf(stderr, "Replay MP4 saved\n");
    {
        std::lock_guard<std::mutex> lock(mutex);
        queue.clear();
        queuedBytes = 0;
        status = ok ? 3 : -1;
    }
    return 0;
}
} // namespace

extern "C" int Port_ReplayVideoAvailable(void) { return 1; }
extern "C" int Port_ReplayVideoActive(void) { return status == 1; }
extern "C" int Port_ReplayVideoStart(void) {
    if (status == 1 || status == 2) return 0;
    if (worker) SDL_WaitThread(worker, nullptr);
    static const int cleanup = std::atexit(stopAtExit);
    (void)cleanup;
    std::lock_guard<std::mutex> lock(mutex);
    firstFrame = 0;
    lastFrame = 0;
    closing = false;
    status = 1;
    worker = SDL_CreateThread(encode, "Replay MP4", nullptr);
    if (!worker) status = -1;
    return worker != nullptr;
}
extern "C" void Port_ReplayVideoStop(void) {
    std::lock_guard<std::mutex> lock(mutex);
    if (status != 1) return;
    status = 2;
    closing = true;
    ready.notify_one();
}
extern "C" int Port_ReplayVideoStatus(void) { return status; }
extern "C" int Port_ReplayVideoDue(void) {
    return status == 1 && (!firstFrame || SDL_GetTicksNS() - lastFrame >= interval - 2000000);
}
extern "C" void Port_ReplayVideoFrame(const void *pixels, int w, int h, int pitch, int bgra, int flipped) {
    if (!pixels || w < 2 || h < 2 || status != 1) return;
    static int width, height;
    std::lock_guard<std::mutex> lock(mutex);
    if (status != 1 || queuedBytes >= queueLimit) return;
    const auto now = SDL_GetTicksNS();
    if (!firstFrame) {
        const int scaledWidth = w > 1920 || h > 1080 ? SDL_min(1920, int(1080LL*w/h)) : w;
        width = SDL_max(2, scaledWidth&~1);
        height = SDL_max(2, int(1LL*h*scaledWidth/w)&~1);
        firstFrame = now;
    }
    lastFrame = now;
    Packet p{{}, now-firstFrame.load(), width, height, 0, 0};
    p.data.resize(size_t(width) * height * 4);
    for (int y = 0; y < height; ++y) {
        const int row = y*h/height;
        const auto *in = static_cast<const Uint8 *>(pixels) + size_t(flipped ? h-1-row : row)*pitch;
        auto *out = p.data.data() + size_t(y)*width*4;
        if (bgra && width == w) memcpy(out, in, width*4);
        else for (int x = 0; x < width; ++x, out += 4) {
            const auto *pixel = in + (x*w/width)*4;
            out[0] = pixel[bgra ? 0 : 2];
            out[1] = pixel[1];
            out[2] = pixel[bgra ? 2 : 0];
            out[3] = 255;
        }
    }
    queuedBytes += p.data.size();
    queue.push_back(std::move(p));
    ready.notify_one();
}
extern "C" void Port_ReplayVideoAudio(const float *samples, int bytes, int rate, int channels) {
    if (status != 1 || !firstFrame || bytes <= 0) return;
    std::lock_guard<std::mutex> lock(mutex);
    if (status != 1 || !firstFrame || queuedBytes >= queueLimit) return;
    const auto *start = reinterpret_cast<const Uint8 *>(samples);
    queue.push_back({{start, start+bytes}, 0, 0, 0, rate, channels});
    queuedBytes += bytes;
    ready.notify_one();
}
#else
extern "C" int Port_ReplayVideoAvailable(void) { return 0; }
extern "C" int Port_ReplayVideoActive(void) { return 0; }
extern "C" int Port_ReplayVideoStart(void) { return 0; }
extern "C" int Port_ReplayVideoStatus(void) { return 0; }
extern "C" int Port_ReplayVideoDue(void) { return 0; }
extern "C" void Port_ReplayVideoStop(void) {}
extern "C" void Port_ReplayVideoFrame(const void *, int, int, int, int, int) {}
extern "C" void Port_ReplayVideoAudio(const float *, int, int, int) {}
#endif
