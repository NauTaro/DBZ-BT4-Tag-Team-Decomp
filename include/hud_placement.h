#ifndef HUD_PLACEMENT_H
#define HUD_PLACEMENT_H

enum HudPlacement {
    HUD_PLACE_NONE,
    HUD_PLACE_GAUGE_1, HUD_PLACE_GAUGE_2, HUD_PLACE_TEAM_1, HUD_PLACE_TEAM_2,
    HUD_PLACE_CLOCK, HUD_PLACE_PROMPT_1, HUD_PLACE_PROMPT_2, HUD_PLACE_COMBO_1, HUD_PLACE_COMBO_2,
    HUD_PLACE_NOTICE, HUD_PLACE_CAPTION, HUD_PLACE_ONLINE_1, HUD_PLACE_ONLINE_2,
    HUD_PLACE_TAG_GAUGE_1, HUD_PLACE_TAG_GAUGE_2, HUD_PLACE_TAG_ALLIES_1, HUD_PLACE_TAG_ALLIES_2,
    HUD_PLACE_TAG_RADAR, HUD_PLACE_TAG_TARGET, HUD_PLACE_TAG_ASSIST_1, HUD_PLACE_TAG_ASSIST_2,
    HUD_PLACE_TAG_CLOCK, HUD_PLACE_TAG_REVIVAL, HUD_PLACE_TAG_ONLINE_1, HUD_PLACE_TAG_ONLINE_2,
    HUD_PLACE_TAG_SPECTATOR, HUD_PLACE_SPARRING, HUD_PLACE_COUNT
};

#define HUD_PLACEMENT_MARKER 0x180

#ifdef __cplusplus
extern "C" {
#endif
void Port_HudGetOffset(int group, int *x, int *y);
#ifdef __cplusplus
}
#endif
#ifndef PORT
#define HudPlaceDraw(group, draw) do { draw; } while (0)
#endif

#if defined(PORT) && !defined(__cplusplus)
extern void Port_GsMarker(int effect);
#define HudPlaceDraw(group, draw) do {                            \
    Port_GsMarker(HUD_PLACEMENT_MARKER + (group));                 \
    draw;                                                        \
    Port_GsMarker(HUD_PLACEMENT_MARKER);                           \
} while (0)
#endif

#endif
