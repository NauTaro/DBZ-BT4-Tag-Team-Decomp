#ifndef BT4_MENU_PANEL_LAYOUT_H
#define BT4_MENU_PANEL_LAYOUT_H

/* Horizontal three-slice expansion of native message panels. Work on the
 * triangles rather than changing their UVs: this also preserves palette,
 * alpha, replacement textures and masks on both GPU back ends. */
static Vtx menu_panel_lerp(Vtx a, Vtx b, float x) {
    float t = (x - a.x) / (b.x - a.x);
    Vtx v = a;
    v.x = x; v.y += (b.y - a.y) * t; v.z += (b.z - a.z) * t;
    v.s += (b.s - a.s) * t; v.t += (b.t - a.t) * t; v.q += (b.q - a.q) * t;
    v.r = (uint8_t)(a.r + (b.r - a.r) * t + .5f);
    v.g = (uint8_t)(a.g + (b.g - a.g) * t + .5f);
    v.b = (uint8_t)(a.b + (b.b - a.b) * t + .5f);
    v.a = (uint8_t)(a.a + (b.a - a.a) * t + .5f);
    return v;
}

static int menu_panel_clip(const Vtx *in, int count, Vtx *out, float x, int after) {
    int n = 0;
    for (int i = 0; i < count; ++i) {
        Vtx a = in[(i + count - 1) % count], b = in[i];
        int ai = after ? a.x >= x : a.x <= x, bi = after ? b.x >= x : b.x <= x;
        if (ai != bi) out[n++] = menu_panel_lerp(a, b, x);
        if (bi) out[n++] = b;
    }
    return n;
}

static unsigned menu_panel_slice(Vtx *out, const Vtx *source, unsigned count,
                                  float lo, float hi, float scale, float shift) {
    unsigned written = 0;
    for (unsigned i = 0; i + 2 < count; i += 3) {
        Vtx a[8], b[8];
        int n = menu_panel_clip(source + i, 3, a, lo, 1);
        n = menu_panel_clip(a, n, b, hi, 0);
        for (int j = 0; j < n; ++j) b[j].x = (b[j].x + shift) * scale;
        for (int j = 1; j + 1 < n; ++j) {
            out[written++] = b[0]; out[written++] = b[j]; out[written++] = b[j + 1];
        }
    }
    return written;
}

static unsigned menu_panel_expand(Vtx *out, const Vtx *source, unsigned count, float scale) {
    float extra = 512.0f / scale - 512.0f;
    unsigned n = menu_panel_slice(out, source, count, -4096, 96, scale, 0);
    /* The middle's original dots and border are repeated, not stretched. */
    for (float x = 96; x < 416 + extra; x += 320) {
        float room = 416 + extra - x;
        n += menu_panel_slice(out + n, source, count, 96, room < 320 ? 96 + room : 416, scale, x - 96);
    }
    n += menu_panel_slice(out + n, source, count, 416, 4096, scale, extra);
    return n;
}
#endif
