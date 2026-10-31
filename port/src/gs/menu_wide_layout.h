#ifndef BT4_MENU_WIDE_LAYOUT_H
#define BT4_MENU_WIDE_LAYOUT_H

/* Panoramic menus and battle HUD use the full page. Do not narrow their
 * vertices or scissor to a centered 4:3 region a second time. */
static inline int Port_MenuWideStretch(int menu_mode, int menus_wide, int hud) {
    return menus_wide && (menu_mode || hud);
}

static inline float Port_MenuWideScaleX(int aspect_milli) {
    return aspect_milli > 1333 ? 1333.333333f / (float)aspect_milli : 1.0f;
}

static inline float Port_MenuWideTransformX(float x, float pivot, float scale) {
    return pivot + (x - pivot) * scale;
}

#endif
