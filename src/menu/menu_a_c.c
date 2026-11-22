#include "common.h"
#include "menu/menu_a.h"
#include "sys/pad.h"
#ifdef PORT
#include <stdio.h>
#include <stdlib.h>
#include "menu/bt4_title_event.h"
#include "menu/port_screen.h"
#include "menu/tbo_art.h"
extern void Port_MenuPage(int screen); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
extern int Port_PageAspectMilli(void);
extern int Port_GameProfileLanguage(void);
extern int Port_GameProfileEnglish(void);
extern int Port_NetFixedStart(void);
extern int Adx_PlayTitleVoice(s32 asset);
extern s32 Voice_IsStopped(s32 voice);
extern void Bgm_SetVolume(s32 volume);

static s32 sTitleEvent, sTitleMusicVolume;

static void Title_EventDraw(void) {
    const f32 picture = 16.0f / 9.0f;
    f32 aspect = Port_PageAspectMilli() / 1000.0f;
    f32 height = aspect < picture ? 448.0f * aspect / picture : 448.0f;
    f32 visible = aspect > picture ? picture / aspect : 1.0f;
    f32 top = (448.0f - height) * 0.5f;
    f32 left, y;
    s32 language = Port_GameProfileLanguage();
    char *start = language == 2 ? "Pressione START" : Port_GameProfileEnglish() ? "Press START" : "Pulsa START";
    char *newGame = language == 2 ? "Novo jogo" : Port_GameProfileEnglish() ? "New Game" : "Nueva partida";
    char *resume = language == 2 || !Port_GameProfileEnglish() ? "Continuar" : "Continue";

    Ps_PageSet();
    Ps_SetShift(0);
    Ps_SpriteScreen(TBO_FUKKATSU_TITLE, 0, top, 512, top + height,
                    0, (1 - visible) * 0.5f, 1, (1 + visible) * 0.5f, PS_PLAIN, 0x80);
    left = Ps_Left() + 23.0f;
    y = top + height * 0.31f;
    Ps_Align(0);
    Ps_TextRaw(left, y, "DRAGON BALL Z", 0.81f, 172.0f, 2);
    Ps_TextRaw(left, y + 20, "BUDOKAI TENKAICHI 4", 1.22f, 188.0f, 2);
    Ps_TextRaw(left, y + 43, "TAG TEAM DECOMP", 0.74f, 150.0f, 2);
    Ps_TextEx(left, y + 64, "FUKKATSU NO F", 0.92f, 150.0f,
              Ps_Rgba(255, 234, 153, 255), Ps_Rgba(244, 139, 65, 255), Ps_Rgba(32, 13, 25, 255));
    if (gTitle->state == 2) {
        s32 i;
        for (i = 0; i < gTitle->itemCount; ++i) {
            f32 row = y + 100 + i * 28.0f;
            Ps_Pill(i == gTitle->cursor ? TBO_KIT_SEL : TBO_KIT_BAR,
                    left, row, left + 138.0f, row + 23.0f, 0x80);
            Ps_TextRaw(left + 12, row + 2, i == 0 ? newGame : resume, 0.86f, 114.0f, 2);
        }
    } else {
        Ps_Button(7, left + 9, y + 112, 15);
        Ps_TextRaw(left + 25, y + 103, start, 0.86f, 135.0f, 2);
    }
}

static int Title_EventVoice(s32 asset) {
    return sTitleEvent && Adx_PlayTitleVoice(asset);
}

static void Title_EventMix(void) {
    s32 target = Voice_IsStopped(0) ? 0x40 : 0x30;
    if (sTitleMusicVolume == target) return;
    sTitleMusicVolume += target < sTitleMusicVolume - 1 ? -2 : target < sTitleMusicVolume ? -1 : 1;
    Bgm_SetVolume(sTitleMusicVolume);
}
#endif

/* The screen's work (overlay .data, 0x3B0EB8). */
Title *gTitle = NULL;

/*
 * Title: the title screen (progress mode 1), 0x336FC0..0x338020. A still picture for 300 frames, then the
 * "press start" movie and a one- or two-item menu (new game / continue) that runs the memory-card flow. After
 * 1800 idle frames it leaves for the attract demo (twice) or the opening movie (every third time).
 */

#define TITLE_VOICE_BASE 0x86F9

#ifdef PORT
extern int gPortOverlayOpen; /* port/src/gs/gs_input.c: settings or quit dialog owns input */
extern int Ui_QuitAsk(void);
#endif

/* What ends the screen: the movie plays out for 15 frames, then the fade starts. */
#define TITLE_LEAVE() \
    gTitle->flags |= TITLE_CHOSEN; \
    gTitle->flags |= TITLE_LEAVING; \
    gTitle->timer = 15

/* Counts an idle frame; at the limit leaves with result 1 (attract demo set up) or 2 (opening movie). */
void Title_Idle(s32 *result) {
    if (++gTitle->idle > TITLE_IDLE_FRAMES) {
        gTitle->idle = 0;
        TITLE_LEAVE();
        switch (gProgress->demoCount) {
        case 0:
        case 1:
            Demo_SetupBattle();
            *result = 1;
            gProgress->demoCount++;
            break;
        default:
            *result = 2;
            gProgress->demoCount = 0;
            break;
        }
    }
}

#define TT_RES(n) \
    res = (MTexRes *)MPACK_AT(gTitle->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 1) and starts the dialog and card modules. */
void Title_Init(s32 section) {
    MTexRes *res = NULL;

    if (gMenuArc1 == NULL) {
        gMenuArc1 = File_LoadSync(gProgress->baseFile + 1, NULL, 0);
    }
    gTitle = Heap_Alloc(0xD0, 0x20, 0, 2);
    memset(gTitle, 0, 0xD0);
    gTitle->pack = (u32 *)MPACK_AT(gMenuArc1, section);
    gTitle->res = Sprite_Unpack(gTitle->pack, NULL, NULL);
    TT_RES(1);
    gTitle->bg = res;
    TT_RES(2);
    gTitle->tex[18] = MTEX(res, 0);
    gTitle->tex[20] = MTEX(res, 1);
    gTitle->tex[22] = MTEX(res, 2);
    gTitle->tex[21] = MTEX(res, 3);
    gTitle->tex[17] = MTEX(res, 4);
    gTitle->tex[19] = MTEX(res, 5);
    TT_RES(3);
    gTitle->tex[10] = MTEX(res, 0);
    gTitle->tex[11] = MTEX(res, 1);
    gTitle->tex[12] = MTEX(res, 2);
    gTitle->tex[13] = MTEX(res, 3);
    gTitle->tex[14] = MTEX(res, 4);
    gTitle->tex[15] = MTEX(res, 5);
    gTitle->tex[16] = MTEX(res, 6);
    TT_RES(14);
    gTitle->logo = res;
    TT_RES(5);
    gTitle->tex[23] = MTEX(res, 0);
    TT_RES(6);
    gTitle->tex[4] = MTEX(res, 0);
    TT_RES(7);
    gTitle->tex[3] = MTEX(res, 0);
    TT_RES(8);
    gTitle->tex[6] = MTEX(res, 0);
    gTitle->tex[9] = MTEX(res, 1);
    TT_RES(9);
    gTitle->tex[5] = MTEX(res, 0);
    gTitle->tex[8] = MTEX(res, 1);
    gTitle->tex[7] = MTEX(res, 2);
    TT_RES(10);
    gTitle->tex[0] = MTEX(res, 0);
    gTitle->tex[1] = MTEX(res, 1);
    gTitle->tex[2] = MTEX(res, 2);
    Flash_Create(&gTitle->flash[0], MPACK_AT(gTitle->res, 11), gTitle->tex);
    Flash_Play(&gTitle->flash[0], 1);
    Dialog_Init(MPACK_AT(gTitle->res, 12), NULL, 0);
    McFlow_Init(1);
    if (gProgress->flags & MPROG_SAVE_LOADED) {
        gTitle->itemCount = 2;
        gTitle->cursor = 1;
    } else {
        gTitle->itemCount = 1;
        gTitle->cursor = 0;
    }
    gTitle->voice = Rand_Range(6);
}

/* Frees the screen. */
void Title_Term(void) {
    s32 i;

    Dialog_Term();
    McFlow_Term();
    for (i = 0; i < TITLE_FLASH_NUM; i++) {
        Flash_Destroy(&gTitle->flash[i]);
    }
    if (gTitle->res != NULL) {
        Heap_Free(gTitle->res);
        gTitle->res = NULL;
    }
    if (gTitle != NULL) {
        Heap_Free(gTitle);
        gTitle = NULL;
    }
}

/* Draws the background, the two scrolling cloud layers, the menu plates and the movie. */
void Title_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    s32 i;
    MFlash *flash;

#ifdef PORT
    if (sTitleEvent) {
        Port_MenuPage(1);
        Title_EventDraw();
        Port_MenuPage(0);
        return;
    }
    Port_MenuPage(1); /* widescreen menus: this screen's page (PORT_MENU_SCREEN in port/src/gs_marker.c), its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gTitle->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gTitle->bg, 0, 0, 0x80);
#endif
    flash = &gTitle->flash[0];
    for (i = 0; i < 2; i++) {
        uv.y0 = 0;
        uv.y1 = 0x100;
        uv.x0 = 0;
        uv.x1 = 0x200;
        sprintf(name, "mc_kumo_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        FlashAnim_Scroll(flash, &ref, &uv, &gTitle->cloud[i], NULL, i ? -1.0666667f : -0.71111111f, 0.0f);
    }
    for (i = 0; i < 2; i++) {
        uv.y0 = i * 0x20;
        uv.y1 = uv.y0 + 0x20;
        uv.x0 = 0;
        uv.x1 = 0x200;
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetColor(flash, &ref, i < gTitle->itemCount ? 1.0f : 0.5f);
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    for (i = 0; i < TITLE_FLASH_NUM; i++) {
        Flash_Draw(&gTitle->flash[i]);
    }
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/* Sends a clip of movie `idx` to a label: kind 1 the "press start" plate, kind 2 the plate under the cursor. */
void Title_ClipGoto(s32 idx, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gTitle->flash[idx];

    switch (kind) {
    case 1:
        sprintf(name, "mc_press_start_plate");
        break;
    case 2:
        sprintf(name, "mc_menu_plate_%d", gTitle->cursor + 1);
        break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Steps the movie and plays the sound it asks for. */
void Title_Update(void) {
    s32 i;

    for (i = 0; i < TITLE_FLASH_NUM; i++) {
        Flash_Advance(&gTitle->flash[i]);
    }
    if (gTitle->flash[0].se & 1) {
        Snd_PlaySe(2, 0x37);
    }
}

/* Card flow callback 0 (done): the save exists now; leave with result 0. */
void Title_OnCardDone(s32 *result) {
    if (result != NULL) {
        *result = 0;
    }
    TITLE_LEAVE();
}

/* Card flow callback 1 (not done): back to the menu. */
void Title_OnCardCancel(void) {
    Title_ClipGoto(0, 2, "fl_on_start");
}

/* Pad 0: start skips the intro and opens the menu; up / down, confirm, or quit from the menu on PC. */
void Title_Input(s32 *result) {
#ifdef PORT
    if (gPortOverlayOpen) {
        return;
    }
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_TAG_VIDEO_TEST")){
        static s32 state=-1,tick;
        if(state!=gTitle->state){state=gTitle->state;tick=0;}
        if(++tick==1)fprintf(stderr,"tag video: title state=%d\n",state);
        if(state<2 && tick>=120 && tick%60==0)gPad[0].gamePressed=PADG_START;
        if(state==2 && tick>=90 && tick%30==0){
            if(gTitle->itemCount>1 && gTitle->cursor==0)gPad[0].gameRepeat=PADG_DOWN;
            else gPad[0].gamePressed=PADG_CROSS;
        }
    }
#endif
    if (gTitle->state == 0) {
        if (gPad[0].gamePressed & 0x1000) {
            Flash_GotoLabel(&gTitle->flash[0], "fl_skip", 1);
            gTitle->state = 1;
            Snd_PlaySe(1, 1);
            return;
        }
    }
    if (!(gTitle->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gTitle->flags & TITLE_STARTED)) {
        Title_ClipGoto(0, 1, "fl_on_start");
        gTitle->state = 1;
        gTitle->flags |= TITLE_STARTED;
        s32 line = Rand_Range(7) != 0 ? gTitle->voice : 0x40;
#ifdef PORT
        if (!Title_EventVoice(BT4_TITLE_GREETING) && !sTitleEvent)
#endif
        {
            Voice_PlayWithSubtitle(NULL, TITLE_VOICE_BASE, line);
        }
    }
    switch (gTitle->state) {
    case 1:
        if (gPad[0].gamePressed & 0x1000) {
            gTitle->idle = 0;
            Flash_GotoLabel(&gTitle->flash[0], "fl_menu_in", 1);
            Title_ClipGoto(0, 1, "fl_ok");
            Title_ClipGoto(0, 2, "fl_on_start");
            gTitle->state = 2;
#ifdef PORT
            if (sTitleEvent) Title_EventVoice(BT4_TITLE_CONFIRM);
            else
#endif
            Voice_PlayWithSubtitle(NULL, TITLE_VOICE_BASE, gTitle->voice + 6);
            Snd_PlaySe(2, 0x38);
        } else {
            Title_Idle(result);
        }
        break;
    case 2:
#ifdef PORT
        if (gPad[0].gamePressed & PADG_TRIANGLE) {
            gTitle->idle = 0;
            Ui_QuitAsk();
            break;
        }
#endif
        if (gPad[0].gameRepeat & 8) {
            gTitle->idle = 0;
            if (gTitle->itemCount >= 2) {
                Title_ClipGoto(0, 2, "fl_off_start");
                gTitle->cursor--;
                if (gTitle->cursor < 0) {
                    gTitle->cursor = gTitle->itemCount - 1;
                }
                Title_ClipGoto(0, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gameRepeat & 4) {
            gTitle->idle = 0;
            if (gTitle->itemCount >= 2) {
                Title_ClipGoto(0, 2, "fl_off_start");
                gTitle->cursor++;
                if (gTitle->cursor >= gTitle->itemCount) {
                    gTitle->cursor = 0;
                }
                Title_ClipGoto(0, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (gPad[0].gamePressed & 0x200) {
            gTitle->idle = 0;
            Title_ClipGoto(0, 2, "fl_ok");
            switch (gTitle->cursor) {
            case 0:
                McFlow_Start(3);
                McFlow_SetDoneCb(0, Title_OnCardDone, result);
                McFlow_SetDoneCb(1, Title_OnCardCancel, NULL);
                break;
            case 1:
                *result = 0;
                TITLE_LEAVE();
                break;
            }
            Snd_PlaySe(1, 1);
        } else {
            Title_Idle(result);
        }
        break;
    }
}

#ifdef PORT
extern int Port_Setting(const char *name, int def);
#endif

/* The title screen's own frame loops. Returns 0 to go on to the main menu, 1 when the attract demo was set up
   (the overlay returns and the battle runs), 2 to play the opening movie. */
s32 Title_Run(s32 section) {
    s32 result = 1;
    s32 wait = 0;

    Title_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
#ifdef PORT
    /* A setting of the PC version (F1, UI): the copyright page, a still held for 300 frames that no button
       shortens, is left out. Nothing else happens in its loop. (Not in an online session: a player's setting does
       not change what the two copies compute. A session does not come here as things are.) */
    while (Port_NetFixedStart() || !Port_Setting("skip_copyright", 0)) {
#else
    while (1) {
#endif
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
#ifdef PORT
        Port_MenuPage(51); /* widescreen menus: the logo page at its true shape, black beside it (port/src/gs_marker.c) */
        Sprite_DrawPicture(gTitle->logo, 0, 0, 0x80);
        Port_MenuPage(0);
#else
        Sprite_DrawPicture(gTitle->logo, 0, 0, 0x80);
#endif
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            gTitle->flags |= TITLE_FADED_IN;
        }
        if (ColorFade_IsFadingOut()) {
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gTitle->flags & TITLE_FADED_IN) {
            wait++;
            if (wait > 300) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        }
    }
#ifdef PORT
    {
        char path[512];
        sTitleEvent = !Port_NetFixedStart() && Port_TitleEventPath(BT4_TITLE_ART, 0, path, sizeof(path));
        if (sTitleEvent && Port_TitleEventPath(BT4_TITLE_OST, 0, path, sizeof(path))) Bgm_Play(BT4_TITLE_OST);
        sTitleMusicVolume = 0x40;
    }
#endif
    ColorFade_StartIn(0, 0, 0, 0x14);
    for (;;) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            Title_Update();
        }
        Title_Draw();
        gTitle->mcState = McFlow_Update();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gTitle->flags & TITLE_LEAVING) {
            if (--gTitle->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gTitle->mcState == 0) {
            Title_Input(&result);
        }
#ifdef PORT
        if (sTitleEvent) Title_EventMix();
#endif
    }
    if (result == 0) {
        gProgress->flags |= MPROG_SAVE_LOADED;
    }
    sceGsSyncPath(0, 0);
#ifdef PORT
    sTitleEvent = 0;
#endif
    Title_Term();
    Dma_ResetBuffers();
    return result;
}
