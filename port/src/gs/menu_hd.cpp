/* Edge-preserving menu artwork enlargement. Host renderer only: never changes game data or netplay state. */
#include <cstdint>
#include <cstdlib>
#include "../../third_party/xbrz/xbrz.cpp"

extern "C" uint32_t *Bt4MenuHD_Upscale(const uint32_t *pixels, int w, int h, int factor) {
    if (!pixels || w < 2 || h < 2 || w > 1024 || h > 1024 || factor < 2 || factor > 6) { return nullptr; }
    const size_t count = static_cast<size_t>(w) * h;
    /* Alpha above 0x80 can carry GS pass identifiers. Do not reinterpret those textures. */
    for (size_t i = 0; i < count; ++i) { if ((pixels[i] >> 24) > 128) { return nullptr; } }
    uint32_t *source = static_cast<uint32_t *>(std::malloc(count * sizeof(uint32_t)));
    uint32_t *result = static_cast<uint32_t *>(std::malloc(count * factor * factor * sizeof(uint32_t)));
    if (!source || !result) { std::free(source); std::free(result); return nullptr; }
    for (size_t i = 0; i < count; ++i) {
        uint32_t c = pixels[i], a = c >> 24;
        /* GS RGBA, opaque=128 -> xBRZ ARGB, opaque=255. */
        source[i] = ((a * 255 + 64) / 128 << 24) | ((c & 255) << 16) | (c & 0xFF00) | ((c >> 16) & 255);
    }
    xbrz::scale(factor, source, result, w, h, xbrz::ColorFormat::ARGB);
    std::free(source);
    for (size_t i = 0; i < count * factor * factor; ++i) {
        uint32_t c = result[i], a = c >> 24;
        result[i] = ((a * 128 + 127) / 255 << 24) | ((c & 255) << 16) | (c & 0xFF00) | ((c >> 16) & 255);
    }
    return result;
}
