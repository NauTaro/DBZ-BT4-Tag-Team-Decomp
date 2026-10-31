#include <assert.h>
#include <math.h>
#include "../../src/gs/menu_wide_layout.h"

static int close_to(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

int main(void) {
    assert(Port_MenuWideStretch(1, 1, 0)); /* every menu page fills the viewport */
    assert(Port_MenuWideStretch(0, 1, 1)); /* battle HUD fills it too */
    assert(!Port_MenuWideStretch(0, 1, 0)); /* world effects retain their rules */
    assert(!Port_MenuWideStretch(1, 0, 1)); /* other display modes retain theirs */

    assert(close_to(Port_MenuWideScaleX(1333), 1.0f));
    assert(close_to(Port_MenuWideScaleX(1600), 0.833333f));
    assert(close_to(Port_MenuWideScaleX(1777), 0.75f));
    assert(close_to(Port_MenuWideScaleX(2333), 0.571429f));
    assert(close_to(Port_MenuWideScaleX(2667), 0.5f));

    {
        /* 16:10 is exact in integer milli-aspect units. 16:9 is rounded to
         * 1777/1778, so expecting exact 64-pixel margins there is incorrect. */
        float scale = Port_MenuWideScaleX(1600);
        float left = Port_MenuWideTransformX(0.0f, 256.0f, scale);
        float center = Port_MenuWideTransformX(256.0f, 256.0f, scale);
        float right = Port_MenuWideTransformX(512.0f, 256.0f, scale);
        assert(close_to(left, 42.666667f));
        assert(close_to(center, 256.0f));
        assert(close_to(right, 469.333333f));
        assert(close_to(right - left, 426.666667f));
    }

    return 0;
}
