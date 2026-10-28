// The setup window of the PC port: pick your disc image, watch the steps, press Play.
//
// A small native program (SDL3 + Dear ImGui, the same library and look as the game's F1 settings window). It does
// none of the work itself: it runs `python3 install.py --machine --iso <file>` next to it and shows the events
// that script prints (one line each: @step, @note, @ok, @skip, @fail, @missing, @stopped, @done; see install.py).
//
//   build:  port/setup/build.sh   ->  ./BT4decomp-setup
//   test:   BT3_SETUP_ISO=<file> starts the installation at once; BT3_SETUP_SHOT=<prefix> writes the window's
//           picture to <prefix>_<n>.ppm whenever the page or the step changes (no person needed to check it)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mutex>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h" // glViewport, glClear for the OpenGL window (set up by ImGui_ImplOpenGL3_Init)
#include "native.h"

#define GAME "BT4decomp" /* the program this sets up: GAME, GAME.dat; this program is GAME-setup */

enum Page { PAGE_PICK, PAGE_RUN, PAGE_DONE, PAGE_FAILED };
enum State { PENDING, RUNNING, OK, SKIPPED, FAILED };
struct StepRow { std::string title, detail; State state = PENDING; Uint64 start = 0, end = 0; };
struct Missing { std::string name, why, pkg; };

// The steps of the two ways to install: from a source checkout (install.py does the work and needs build tools) and
// in a release folder (BT4decomp and BT4decomp.dat next to this program: native.cpp unpacks the disc itself).
static const char *kSourceSteps[] = {"Requirements", "Imagen ISO", "Datos del juego", "Executable data", "Build", "Prueba del juego"};
static const char *kReleaseSteps[] = {"Disc image", "Game data", "Native data tables", "Self-test"};
static const struct { const char *title, *help; } kHelp[] = {
    {"Disc image", "Verify the LATINO or English BT4 B14 Rev2 disc profile"},
    {"Game data", "Extract and verify the files for the selected disc profile"},
    {"Native data tables", "Prepare the data tables for this executable and disc profile"},
    {"Self-test", "Run a test fight without opening a window"},
    {"European languages", "Spanish, German, French and Italian from the European disc"},
    {"Requirements", "Compilers and tools on this computer"}, {"Imagen ISO", "Comprobación de la ISO y su versión"},
    {"Datos del juego", "Extracción y verificación de modelos, sonidos y textos"}, {"Executable data", "The data tables of the game's programs"},
    {"Build", "The port is compiled (a few minutes)"}, {"Prueba del juego", "Pelea de prueba sin abrir una ventana"}};
static bool sRelease;
static bool sUiEnglish=false,sPackage=false;
static int sInstallMask=1,sPackagePercent=0;
static std::string sGithubCode;
static const char *L(const char *es,const char *en){return sUiEnglish?en:es;}


static const char *step_help(const std::string &title) {
    for (auto &h : kHelp) {
        if (title == h.title) {
            return h.help;
        }
    }
    return "";
}

static SDL_Window *sWindow;
static SDL_GPUDevice *sDevice;
// The window is drawn through SDL's GPU layer (Vulkan on Linux, Direct3D 12 on Windows) or, where that cannot
// start (no Vulkan driver, a graphics chip too old for it: reported for Intel Ivy Bridge), through OpenGL.
// BT3_GPU_API=gl asks for OpenGL at once, as it does for the game.
static SDL_GLContext sGl;
static bool sUseGl;
static Page sPage = PAGE_PICK;
static char sIso[1024];
// Optional: a disc image of the European release (SLES-54945), for Spanish, German, French and Italian. The game is
// always set up from the USA one; from this one only the languages' archive entries are taken (native.cpp step_eu,
// install.py --eu-iso).
static char sIsoEu[1024];
static bool sEuOnly;    // only the languages are added (the game was set up before: the "Listo para jugar" page)
static int sDialogFor;  // which field the file dialog fills: 0 the USA image, 1 the European one
static std::string sRoot, sError, sLauncher, sLine;
static std::vector<StepRow> sRows;
static std::vector<Missing> sMissing;
static SDL_Process *sProc;
static bool sNative; // the native installer is at work (release folder)
static std::mutex sDialogLock;
static std::string sDialogResult;
static bool sDialogReady, sQuit;
static Uint64 sRunStart, sRunEnd;
static const ImVec4 kAccent(0.96f, 0.55f, 0.13f, 1.0f), kGreen(0.36f, 0.80f, 0.45f, 1.0f), kRed(0.93f, 0.36f, 0.33f, 1.0f),
    kDim(0.60f, 0.63f, 0.70f, 1.0f);

static bool file_exists(const char *path) {
    SDL_PathInfo info;
    return path[0] != '\0' && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

// The folder to install into: a release folder (this program's own), or the source checkout that holds install.py.
static std::string find_root() {
    std::string dir = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    if ((file_exists((dir + GAME).c_str()) || file_exists((dir + GAME ".exe").c_str())) && file_exists((dir + GAME ".dat").c_str())) {
        sRelease = true; // a release folder: the finished program is here, only the game data is missing
        return dir;
    }
    for (int up = 0; up < 4; up++) {
        // The installer lives under tbo/setup to keep the portable root clean.
        if (file_exists((dir + GAME ".exe").c_str()) && file_exists((dir + GAME ".dat").c_str())) {
            sRelease = true;
            return dir;
        }
        if (file_exists((dir + "install.py").c_str())) {
            return dir;
        }
        dir += "../";
    }
    return "";
}

static void style() {
    ImGuiStyle &st = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    st.WindowRounding = 0.0f;
    st.FrameRounding = 6.0f;
    st.GrabRounding = 6.0f;
    st.WindowPadding = ImVec2(28.0f, 22.0f);
    st.FramePadding = ImVec2(12.0f, 8.0f);
    st.ItemSpacing = ImVec2(10.0f, 10.0f);
    st.WindowBorderSize = 0.0f;
    ImVec4 *c = st.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.025f, 0.025f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.17f, 0.23f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.24f, 0.32f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.29f, 0.38f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.27f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.96f, 0.55f, 0.13f, 0.55f);
    c[ImGuiCol_ButtonActive] = kAccent;
    c[ImGuiCol_PlotHistogram] = kAccent;
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.96f, 0.55f, 0.13f, 0.35f);
}

// ---------------------------------------------------------------------------------------------- the engine script

// The rows as the run numbers its steps: the European languages, when asked for, before the self-test (native.cpp
// work, install.py main); only them when they are added to a game set up before.
static void reset_rows() {
    std::vector<std::string> titles;
    if (sRelease) {
        titles.assign(kReleaseSteps, kReleaseSteps + sizeof(kReleaseSteps) / sizeof(kReleaseSteps[0]));
    } else {
        titles.assign(kSourceSteps, kSourceSteps + sizeof(kSourceSteps) / sizeof(kSourceSteps[0]));
    }
    if (sEuOnly) {
        titles.assign(1, "European languages");
    } else if (sIsoEu[0] != '\0') {
        titles.insert(titles.end() - 1, "European languages");
    }
    if(sRelease && Native_BundledEnglish(sRoot)){
        if(!sIso[0] && !sEuOnly)titles.clear();
        titles.push_back("English language");titles.push_back("English self-test");
    }
    if(sRelease&&sInstallMask==3){titles.push_back("Language package");titles.push_back("Self-test");}
    sPackage=false;sPackagePercent=0;sGithubCode.clear();
    sRows.assign(titles.size(), StepRow());
    for (size_t i = 0; i < titles.size(); i++) {
        sRows[i].title = titles[i];
    }
}

static void start_install() {
    static std::string iso, euIso;
    const char *args[10];
    SDL_PropertiesID props = SDL_CreateProperties();
    std::string script = sRoot + "install.py";
    int n = 0;

    if (sRelease) {
        SDL_DestroyProperties(props);
        reset_rows();
        sMissing.clear();
        sError.clear();
        sLauncher.clear();
        sRunStart = SDL_GetTicks();
        sNative = true;
        Native_Start(sRoot,sEuOnly?"":sIso,"",sInstallMask);
        sPage = PAGE_RUN;
        return;
    }
    iso = sIso;
    args[n++] = "python3";
    args[n++] = script.c_str();
    args[n++] = "--machine";
    args[n++] = "--iso";
    args[n++] = iso.c_str();
    if (sIsoEu[0] != '\0') {
        euIso = sIsoEu;
        args[n++] = "--eu-iso";
        args[n++] = euIso.c_str();
    }
    if (getenv("BT3_SETUP_ARGS") != NULL) { // one extra argument for testing, such as --skip-test
        args[n++] = getenv("BT3_SETUP_ARGS");
    }
    args[n] = NULL;
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, sRoot.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    reset_rows();
    sMissing.clear();
    sError.clear();
    sLine.clear();
    sLauncher.clear();
    sProc = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    sRunStart = SDL_GetTicks();
    if (sProc == NULL) {
        sError = std::string("Could not start python3: ") + SDL_GetError() + "\nPython 3 has to be installed to set the game up.";
        sPage = PAGE_FAILED;
        return;
    }
    sPage = PAGE_RUN;
}

static StepRow *current() {
    for (auto &r : sRows) {
        if (r.state == RUNNING) {
            return &r;
        }
    }
    return NULL;
}

static std::string unescape(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') {
            out += '\n';
            i++;
            while (i + 1 < s.size() && s[i + 1] == ' ') { // the terminal version indents continuation lines
                i++;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static void event(const std::string &line) {
    if (line.empty() || line[0] != '@') {
        return;
    }
    size_t sp = line.find(' ');
    std::string kind = line.substr(1, sp == std::string::npos ? std::string::npos : sp - 1);
    std::string text = sp == std::string::npos ? "" : unescape(line.substr(sp + 1));
    StepRow *cur = current();

    if(kind=="package"){sPackage=true;sPackagePercent=atoi(text.c_str());sGithubCode.clear();return;}
    if(kind=="github"){sGithubCode=text;return;}
    if (kind == "step") {
        int i = atoi(text.c_str());
        if (i >= 1 && i <= (int)sRows.size()) {
            sRows[i - 1].state = RUNNING;
            sRows[i - 1].start = SDL_GetTicks();
            sRows[i - 1].detail.clear();
        }
    } else if (cur != NULL && kind == "note") {
        cur->detail = text;
    } else if (cur != NULL && (kind == "ok" || kind == "skip" || kind == "fail")) {
        cur->state = kind == "ok" ? OK : kind == "skip" ? SKIPPED : FAILED;
        cur->end = SDL_GetTicks();
        cur->detail = kind == "fail" ? "" : text;
    } else if (kind == "missing") {
        Missing m;
        size_t a = text.find('|'), b = text.find('|', a == std::string::npos ? 0 : a + 1);
        if (a != std::string::npos && b != std::string::npos) {
            m.name = text.substr(0, a);
            m.why = text.substr(a + 1, b - a - 1);
            m.pkg = text.substr(b + 1);
            sMissing.push_back(m);
        }
    } else if (kind == "stopped") {
        sError = text;
    } else if (kind == "done") {
        sLauncher = text;
    }
}

static void play_game() {
        const char *args[2] = {sLauncher.c_str(), NULL};
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_Environment *env = SDL_CreateEnvironment(true);
        SDL_SetEnvironmentVariable(env, "BT3_GS", "gpu", true); // the window and the GPU renderer
        for (const char *key : {"BT3_DEMO", "BT3_REPLAY", "BT3_DATA", "BT3_SETTINGS", "BT3_SAVES", "BT3_PAD_PLAY", "BT3_PAD_REC", "BT3_NET_SESSION", "BT3_NET_HOST", "BT3_NET_JOIN", "BT3_LOBBY", "BT3_NET_SCRIPT", "BT4_TEST_MODE", "BT4_TEST_TECH", "BT4_RENDER_TEST"}) {
            SDL_UnsetEnvironmentVariable(env, key);
        }
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
        SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, sRoot.c_str());
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
        SDL_Process *game = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
        SDL_DestroyEnvironment(env);
        if (game != NULL) {
            SDL_DestroyProcess(game);
            sQuit = true;
        } else {
            sError = std::string("No se pudo abrir BT4: ") + SDL_GetError();
            sPage = PAGE_FAILED;
        }
}

static void pump_install() {
    char buf[4096];
    SDL_IOStream *out;
    int code = 0;

    if (sNative) {
        std::string line;
        bool ended = !Native_Running(); // asked before the queue is emptied: nothing is left behind
        while (Native_Poll(line)) {
            event(line);
        }
        if (ended) {
            sNative = false;
            sRunEnd = SDL_GetTicks();
            for (auto &r : sRows) {
                if (r.state == RUNNING) {
                    r.state = FAILED;
                    r.end = sRunEnd;
                }
            }
            if (!sLauncher.empty()) {
                sPage = PAGE_DONE;
            } else {
                if (sError.empty()) {
                    sError = "La instalación se detuvo. Revisa el registro para ver el motivo.";
                }
                sPage = PAGE_FAILED;
            }
        }
        return;
    }
    if (sProc == NULL) {
        return;
    }
    out = SDL_GetProcessOutput(sProc);
    for (;;) {
        size_t n = out != NULL ? SDL_ReadIO(out, buf, sizeof(buf)) : 0;
        if (n == 0) {
            break;
        }
        for (size_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                event(sLine);
                sLine.clear();
            } else if (buf[i] != '\r') {
                sLine += buf[i];
            }
        }
    }
    if (SDL_WaitProcess(sProc, false, &code)) {
        event(sLine);
        SDL_DestroyProcess(sProc);
        sProc = NULL;
        sRunEnd = SDL_GetTicks();
        for (auto &r : sRows) {
            if (r.state == RUNNING) {
                r.state = FAILED;
                r.end = sRunEnd;
            }
        }
        if (code == 0 && !sLauncher.empty()) {
            sPage = PAGE_DONE;
        } else {
            if (sError.empty()) {
                sError = "The setup script stopped unexpectedly. The details are in install.log.";
            }
            sPage = PAGE_FAILED;
        }
    }
}

// --------------------------------------------------------------------------------------------------------- pages

static void SDLCALL dialog_done(void *userdata, const char *const *files, int filter) {
    (void)filter;
    std::lock_guard<std::mutex> lock(sDialogLock);
    if (files != NULL && files[0] != NULL) {
        sDialogResult = files[0];
        sDialogFor = userdata != NULL; // (NULL: the USA image's field)
        sDialogReady = true;
    }
}

static void heading(const char *title, const char *sub) {
    ImGui::PushFont(NULL, 30.0f);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::TextWrapped("%s", sub);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
}

static bool big_button(const char *label, bool primary, bool enabled = true) {
    bool pressed;
    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.55f, 0.13f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.63f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.06f, 0.03f, 1.0f));
    }
    ImGui::BeginDisabled(!enabled);
    pressed = ImGui::Button(label, ImVec2(150.0f, 40.0f));
    ImGui::EndDisabled();
    if (primary) {
        ImGui::PopStyleColor(3);
    }
    return pressed;
}

// Buttons of a page sit on one line at the bottom right.
static void bottom_row(int buttons) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float w = buttons * 150.0f + (buttons - 1) * ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + avail.x - w, ImGui::GetCursorPosY() + avail.y - 40.0f));
}

// The European disc image's field (optional). Returns false when something is in it that is not a file.
static bool eu_field() {
    static const SDL_DialogFileFilter filters[] = {{"PS2 disc image", "iso;bin;img"}, {"All files", "*"}};
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::TextUnformatted("European disc image (optional)");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 130.0f);
    ImGui::InputTextWithHint("##isoeu", "the European release (SLES-54945)", sIsoEu, sizeof(sIsoEu));
    ImGui::SameLine();
    if (ImGui::Button("Browse...##eu", ImVec2(120.0f, 0.0f))) {
        SDL_ShowOpenFileDialog(dialog_done, (void *)1, sWindow, filters, 2, NULL, false);
    }
    if (sIsoEu[0] != '\0' && !file_exists(sIsoEu)) {
        ImGui::TextColored(kRed, "El archivo no existe.");
        return false;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::TextWrapped("Adds Spanish, German, French and Italian: menus, battle screen and text, as the European release has them. F1 > UI > Language picks one.");
    ImGui::PopStyleColor();
    return true;
}

static void page_pick() {
    static const SDL_DialogFileFilter filters[]={{"BT4 ISO","iso;bin;img"},{"All files","*"}};
    ImGui::SetNextItemWidth(160);
    int ui=sUiEnglish?1:0;
    if(ImGui::Combo("##interface-language",&ui,"Español\0English\0"))sUiEnglish=ui!=0;
    ImGui::Dummy(ImVec2(0,10));
    heading("DRAGON BALL Z BT4",L("Instalación","Installation"));
    ImGui::PushFont(NULL,22);ImGui::TextUnformatted(L("¿Qué idioma querés instalar?","Which language would you like to install?"));ImGui::PopFont();
    ImGui::Dummy(ImVec2(0,10));
    const char *labels[]={L("Español","Spanish"),L("Inglés","English"),L("Español e inglés","Spanish and English")};
    for(int i=0;i<3;++i){
        bool selected=sInstallMask==i+1;
        if(selected){ImGui::PushStyleColor(ImGuiCol_Button,kAccent);ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(.08f,.05f,.02f,1));}
        if(ImGui::Button(labels[i],ImVec2(i==2?340:165,48)))sInstallMask=i+1;
        if(selected)ImGui::PopStyleColor(2);
        if(i==0)ImGui::SameLine();
    }
    ImGui::Dummy(ImVec2(0,12));
    ImGui::PushStyleColor(ImGuiCol_Text,kDim);ImGui::TextWrapped("%s",sInstallMask==3?
        L("El segundo idioma se descarga desde GitHub. Iniciá sesión si se solicita.","The second language downloads from GitHub. Sign in if requested."):
        L("Textos y voces. Sin selector de idioma al iniciar.","Text and voices. No language selector at startup."));ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0,12));
    ImGui::TextUnformatted(L("Tu ISO de BT4 B14 Rev2","Your BT4 B14 Rev2 ISO"));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-140);
    ImGui::InputTextWithHint("##iso",L("Seleccionar archivo…","Select file…"),sIso,sizeof(sIso));ImGui::SameLine();
    if(ImGui::Button(L("Elegir ISO","Choose ISO"),ImVec2(130,0)))SDL_ShowOpenFileDialog(dialog_done,NULL,sWindow,filters,2,NULL,false);
    bool existing=Native_Installed(sRoot),ok=file_exists(sIso)||(sIso[0]==0&&existing);
    if(existing&&sIso[0]==0)ImGui::TextColored(kDim,"%s",L("Se usará la instalación actual.","Your existing installation will be used."));
    else if(sIso[0]&&!file_exists(sIso))ImGui::TextColored(kRed,"%s",L("No se encuentra ese archivo.","That file could not be found."));
    bottom_row(2);
    if(big_button(L("Cerrar","Close"),false))sQuit=true;
    ImGui::SameLine();
    if(big_button(L("Instalar","Install"),true,ok&&!sRoot.empty())){sEuOnly=false;start_install();}
}

static void state_icon(State st, float t) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + 11.0f, p.y + 11.0f);

    if (st == OK || st == SKIPPED) {
        ImU32 col = ImGui::GetColorU32(kGreen);
        dl->AddCircle(c, 10.0f, col, 32, 1.6f);
        dl->AddLine(ImVec2(c.x - 5.0f, c.y), ImVec2(c.x - 1.5f, c.y + 4.0f), col, 2.0f);
        dl->AddLine(ImVec2(c.x - 1.5f, c.y + 4.0f), ImVec2(c.x + 5.5f, c.y - 4.0f), col, 2.0f);
    } else if (st == FAILED) {
        ImU32 col = ImGui::GetColorU32(kRed);
        dl->AddCircle(c, 10.0f, col, 32, 1.6f);
        dl->AddLine(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x + 4.0f, c.y + 4.0f), col, 2.0f);
        dl->AddLine(ImVec2(c.x - 4.0f, c.y + 4.0f), ImVec2(c.x + 4.0f, c.y - 4.0f), col, 2.0f);
    } else if (st == RUNNING) {
        float a = t * 5.0f;
        dl->AddCircle(c, 10.0f, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f)), 32, 2.0f);
        dl->PathArcTo(c, 10.0f, a, a + 1.9f, 24);
        dl->PathStroke(ImGui::GetColorU32(kAccent), 0, 2.4f);
    } else {
        dl->AddCircle(c, 10.0f, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.18f)), 32, 1.4f);
    }
    ImGui::Dummy(ImVec2(22.0f, 22.0f));
}

static void step_list() {
    float t = (float)SDL_GetTicks() / 1000.0f;
    for (size_t i = 0; i < sRows.size(); i++) {
        StepRow &r = sRows[i];
        char clock[32] = "";
        state_icon(r.state, t);
        ImGui::SameLine(0.0f, 14.0f);
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 1.0f));
        ImGui::TextColored(r.state == PENDING ? kDim : ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s", r.title.c_str());
        ImGui::TextColored(kDim, "%s", !r.detail.empty() ? r.detail.c_str() : step_help(r.title));
        ImGui::PopStyleVar();
        ImGui::EndGroup();
        if (r.state == RUNNING) {
            snprintf(clock, sizeof(clock), "%d s", (int)((SDL_GetTicks() - r.start) / 1000));
        } else if ((r.state == OK || r.state == FAILED) && r.end - r.start >= 2000) {
            snprintf(clock, sizeof(clock), "%d s", (int)((r.end - r.start) / 1000));
        }
        if (clock[0] != '\0') {
            ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - ImGui::CalcTextSize(clock).x);
            ImGui::TextColored(kDim, "%s", clock);
        }
    }
}

static void page_run() {
    ImGui::Dummy(ImVec2(0,70));
    if(!sGithubCode.empty()){
        heading(L("Conectar con GitHub","Connect to GitHub"),L("Autorizá tu cuenta para descargar el idioma.","Authorize your account to download the language."));
        if(sGithubCode!="login"){
            ImGui::PushFont(NULL,36);ImGui::TextUnformatted(sGithubCode.c_str());ImGui::PopFont();
            ImGui::TextUnformatted("github.com/login/device");
            if(ImGui::Button(L("Abrir GitHub","Open GitHub")))SDL_OpenURL("https://github.com/login/device");
        }
    }else{
        heading(sPackage?L("Instalando paquete de idioma","Installing language pack"):L("Instalando BT4","Installing BT4"),"");
        int done=0;for(auto &r:sRows)done+=r.state==OK||r.state==SKIPPED;
        int pct=sPackage?sPackagePercent:done*100/(int)(sRows.empty()?1:sRows.size());
        if(pct<0)pct=0;if(pct>100)pct=100;
        ImGui::PushFont(NULL,44);ImGui::Text("%d%%",pct);ImGui::PopFont();
        ImGui::Dummy(ImVec2(0,18));ImGui::ProgressBar(pct/100.f,ImVec2(-FLT_MIN,16),"");
    }
    bottom_row(1);
    if(big_button(L("Cancelar","Cancel"),false)){Native_Cancel();sError="INSTALL_CANCELLED";}
}

static void page_done() {
    ImGui::Dummy(ImVec2(0,75));
    heading(L("BT4 está listo","BT4 is ready"),sInstallMask==3?L("Español e inglés instalados.","Spanish and English installed."):sInstallMask==2?L("Inglés instalado.","English installed."):L("Español instalado.","Spanish installed."));
    bottom_row(2);
    if(big_button(L("Volver","Back"),false))sPage=PAGE_PICK;
    ImGui::SameLine();if(big_button(L("Jugar","Play"),true))play_game();
}

static const char *error_text(){
    if(sError=="GITHUB_HELPER_MISSING")return L("Falta un archivo del instalador. Extraé el ZIP completo en una carpeta y abrí Install.exe.","An installer file is missing. Extract the entire ZIP into a folder and open Install.exe.");
    if(sError=="LANGUAGE_ISO_MISMATCH")return L("La ISO no corresponde al idioma elegido. Elegí la ISO latina o inglesa correcta.","The ISO does not match the selected language. Choose the matching Spanish or English ISO.");
    if(sError=="GITHUB_ACCESS_REQUIRED")return L("Esta cuenta no tiene acceso a los paquetes privados de BT4. Usá una cuenta autorizada en GitHub.","This account cannot access the private BT4 packages. Use an authorized GitHub account.");
    if(sError=="GITHUB_LOGIN_FAILED")return L("No se completó el acceso a GitHub. Podés volver a intentarlo.","GitHub sign-in did not complete. You can try again.");
    if(sError=="ISO_REQUIRED")return L("Elegí una ISO de BT4 del idioma que querés instalar.","Choose a BT4 ISO in the language you want to install.");
    if(sError=="INSTALL_CANCELLED")return L("Instalación cancelada.","Installation cancelled.");
    if(sError=="LANGUAGE_VERIFY_FAILED")return L("La descarga no se pudo verificar. Volvé a intentarlo.","The download could not be verified. Try again.");
    return L("No se pudo completar la instalación. Revisá el archivo ISO, la conexión y el espacio disponible.","Installation could not complete. Check the ISO, your connection and available disk space.");
}
static void page_failed() {
    heading(L("Instalación detenida","Installation stopped"),error_text());
    bottom_row(3);if(big_button(L("Ver registro","View log"),false))SDL_OpenURL(("file://"+sRoot+"tbo/setup/install.log").c_str());
    ImGui::SameLine();if(big_button(L("Volver","Back"),true))sPage=PAGE_PICK;
    ImGui::SameLine();if(big_button(L("Cerrar","Close"),false))sQuit=true;
}

static void build_ui() {
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("setup", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    switch (sPage) {
    case PAGE_PICK: page_pick(); break;
    case PAGE_RUN: page_run(); break;
    case PAGE_DONE: page_done(); break;
    case PAGE_FAILED: page_failed(); break;
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------------------------------- the window

// BT3_SETUP_SHOT: the picture drawn a second time into a texture and written as a PPM file.
static void shot(ImDrawData *dd, Uint32 w, Uint32 h, int number) {
    SDL_GPUTextureFormat fmt = SDL_GetGPUSwapchainTextureFormat(sDevice, sWindow);
    bool bgr = fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    SDL_GPUTextureCreateInfo ci;
    SDL_GPUTransferBufferCreateInfo ti;
    SDL_GPUColorTargetInfo target;
    SDL_GPUTextureRegion src;
    SDL_GPUTextureTransferInfo dst;
    char name[512];

    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = fmt;
    ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = w;
    ci.height = h;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    SDL_GPUTexture *tex = SDL_CreateGPUTexture(sDevice, &ci);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sDevice);
    ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
    SDL_zero(target);
    target.texture = tex;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
    ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
    SDL_EndGPURenderPass(pass);
    SDL_zero(ti);
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    ti.size = w * h * 4;
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(sDevice, &ti);
    SDL_zero(src);
    src.texture = tex;
    src.w = w;
    src.h = h;
    src.d = 1;
    SDL_zero(dst);
    dst.transfer_buffer = tb;
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(cp, &src, &dst);
    SDL_EndGPUCopyPass(cp);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(sDevice, true, &fence, 1);
    SDL_ReleaseGPUFence(sDevice, fence);
    const Uint8 *px = (const Uint8 *)SDL_MapGPUTransferBuffer(sDevice, tb, false);
    snprintf(name, sizeof(name), "%s_%d.ppm", getenv("BT3_SETUP_SHOT"), number);
    FILE *fp = fopen(name, "wb");
    if (fp != NULL && px != NULL) {
        fprintf(fp, "P6\n%u %u\n255\n", w, h);
        for (Uint32 i = 0; i < w * h; i++) {
            Uint8 rgb[3] = {px[i * 4 + (bgr ? 2 : 0)], px[i * 4 + 1], px[i * 4 + (bgr ? 0 : 2)]};
            fwrite(rgb, 1, 3, fp);
        }
        fclose(fp);
    }
    SDL_UnmapGPUTransferBuffer(sDevice, tb);
    SDL_ReleaseGPUTransferBuffer(sDevice, tb);
    SDL_ReleaseGPUTexture(sDevice, tex);
}

#ifdef _WIN32
// The few functions of Windows used below, declared here: its headers define names this file has (FAILED, ...).
extern "C" {
__declspec(dllimport) wchar_t *__stdcall GetCommandLineW(void);
__declspec(dllimport) wchar_t **__stdcall CommandLineToArgvW(const wchar_t *line, int *count);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned page, unsigned long flags, const wchar_t *wide, int wideLen, char *out, int outLen,
                                                        const char *def, int *usedDef);
__declspec(dllimport) void *__stdcall LocalFree(void *mem);
}

// The program's arguments in UTF-8. Those main() gets on Windows are in the system's code page: a disc image whose
// path has letters outside it arrives with '?' in their place, and one with letters inside it as bytes that are not
// UTF-8, which is what SDL opens files by. Either way the image was "not found". NULL when Windows gives none.
static char **utf8_args(int *count) {
    int n = 0;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &n);
    char **out = wide != NULL ? (char **)SDL_calloc((size_t)n + 1, sizeof(char *)) : NULL;

    for (int i = 0; out != NULL && i < n; i++) {
        int len = WideCharToMultiByte(65001 /* CP_UTF8 */, 0, wide[i], -1, NULL, 0, NULL, NULL);
        out[i] = (char *)SDL_calloc((size_t)(len > 0 ? len : 1), 1);
        if (out[i] == NULL) {
            out = NULL; // (out of memory at start-up: main's own arguments are used)
            break;
        }
        if (len > 0) {
            WideCharToMultiByte(65001, 0, wide[i], -1, out[i], len, NULL, NULL);
        }
    }
    if (wide != NULL) {
        LocalFree(wide);
    }
    *count = n;
    return out;
}
#endif

int main(int argc, char **argv) {
    static const char *fonts[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf",
    };
    ImGui_ImplSDLGPU3_InitInfo info;
    int shots = 0, shotKey = -1;

#ifdef _WIN32
    {
        int count = 0;
        char **args = utf8_args(&count); // (kept for the whole run)
        if (args != NULL) {
            argc = count;
            argv = args;
        }
    }
#endif
    if (argc > 2 && strcmp(argv[1], "--install") == 0) {
        // Without a window: BT4decomp-setup --install <disc.iso> [<European disc.iso>] (an empty first one: only
        // the languages, for a folder set up before). A release folder only; prints the event lines.
        std::string line;
        bool ok = false;
        SDL_Init(0);
        sRoot = find_root();
        if (!sRelease) {
            fprintf(stderr, GAME "-setup --install works in a release folder (" GAME " and " GAME ".dat next to it).\nIn a source checkout: python3 install.py --iso <file>\n");
            return 2;
        }
        Native_Start(sRoot, argv[2], argc > 3 ? argv[3] : "");
        for (bool last = false; !last;) {
            last = !Native_Running(); // one more round after the worker has ended: nothing is left behind
            while (Native_Poll(line)) {
                printf("%s\n", line.c_str());
                fflush(stdout);
                ok = ok || line.rfind("@done", 0) == 0;
            }
            SDL_Delay(50);
        }
        Native_Join();
        return ok ? 0 : 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, GAME "-setup: %s\n", SDL_GetError());
        return 1;
    }
    const char *glsl = "#version 330";
    {
        const char *api = getenv("BT3_GPU_API");
        bool wantGl = api != NULL && (strcmp(api, "gl") == 0 || strcmp(api, "opengl") == 0);
        std::string why;
        if (!wantGl) {
            sWindow = SDL_CreateWindow(GAME " - Setup", 760, 560, SDL_WINDOW_HIGH_PIXEL_DENSITY | (getenv("BT4_RENDER_TEST")?SDL_WINDOW_HIDDEN:0));
            sDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, false, NULL);
            if (sWindow == NULL || sDevice == NULL || !SDL_ClaimWindowForGPUDevice(sDevice, sWindow)) {
                why = SDL_GetError();
                if (sDevice != NULL) {
                    SDL_DestroyGPUDevice(sDevice);
                    sDevice = NULL;
                }
                if (sWindow != NULL) {
                    SDL_DestroyWindow(sWindow);
                    sWindow = NULL;
                }
            }
        }
        if (sDevice == NULL) {
            // OpenGL 3.3 core, and failing that whatever 3.0 context the driver gives
            for (int attempt = 0; attempt < 2 && sGl == NULL; attempt++) {
                SDL_GL_ResetAttributes();
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, attempt == 0 ? 3 : 0);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, attempt == 0 ? SDL_GL_CONTEXT_PROFILE_CORE : 0);
                SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
                glsl = attempt == 0 ? "#version 330" : "#version 130";
                sWindow = SDL_CreateWindow(GAME " - Setup", 760, 560, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_OPENGL | (getenv("BT4_RENDER_TEST")?SDL_WINDOW_HIDDEN:0));
                sGl = sWindow != NULL ? SDL_GL_CreateContext(sWindow) : NULL;
                if (sGl == NULL && sWindow != NULL) {
                    SDL_DestroyWindow(sWindow);
                    sWindow = NULL;
                }
            }
            if (sGl == NULL) {
                fprintf(stderr, GAME "-setup: no window: %s%s%s\nThe same setup runs without a window: " GAME "-setup --install <disc image>\n",
                        why.c_str(), why.empty() ? "" : "; OpenGL: ", SDL_GetError());
                return 1;
            }
            if (!wantGl) {
                fprintf(stderr, GAME "-setup: %s; the window uses OpenGL\n", why.c_str());
            }
            SDL_GL_MakeCurrent(sWindow, sGl);
            SDL_GL_SetSwapInterval(1);
            sUseGl = true;
        } else {
            SDL_SetGPUSwapchainParameters(sDevice, sWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
        }
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    {SDL_Surface *icon=SDL_LoadPNG((sRoot+"fonts/icon_game.png").c_str());if(icon){SDL_SetWindowIcon(sWindow,icon);SDL_DestroySurface(icon);}}
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    // The keyboard reaches every control: Tab and the arrows move, Space or Enter presses. Without this, Tab stops at
    // the text field only and no button can be pressed without a mouse.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    style();
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        if (file_exists(fonts[i])) {
            io.Fonts->AddFontFromFileTTF(fonts[i], 18.0f);
            break;
        }
    }
    if (sUseGl) {
        ImGui_ImplSDL3_InitForOpenGL(sWindow, sGl);
        ImGui_ImplOpenGL3_Init(glsl);
    } else {
        ImGui_ImplSDL3_InitForSDLGPU(sWindow);
        info.Device = sDevice;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(sDevice, sWindow);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        ImGui_ImplSDLGPU3_Init(&info);
    }
    sRoot = find_root();
    if (false && sRelease && Native_Installed(sRoot) && getenv("BT3_SETUP_ISO") == NULL) { // nothing left to do: offer Play
        if(Native_BundledEnglish(sRoot)&&!Native_EnglishInstalled(sRoot)){
            sIso[0]=0;start_install();
        }else{
        reset_rows();
        for (auto &r : sRows) {
            r.state = SKIPPED;
            r.detail = "already done";
        }
        sLauncher = sRoot + (file_exists((sRoot + GAME ".exe").c_str()) ? GAME ".exe" : GAME);
        sPage = PAGE_DONE;
        }
    }
    if (argc > 1) {
        SDL_strlcpy(sIso, argv[1], sizeof(sIso));
    }
    if (getenv("BT3_SETUP_EU_ISO") != NULL) { // (testing, with BT3_SETUP_ISO: the European image too)
        SDL_strlcpy(sIsoEu, getenv("BT3_SETUP_EU_ISO"), sizeof(sIsoEu));
    }
    if (getenv("BT3_SETUP_ISO") != NULL) {
        SDL_strlcpy(sIso, getenv("BT3_SETUP_ISO"), sizeof(sIso));
        if (!sRoot.empty() && file_exists(sIso)) {
            start_install();
        }
    }
    if(getenv("BT4_RENDER_TEST")){
        if(getenv("BT4_SETUP_UI_LANGUAGE"))sUiEnglish=!strcmp(getenv("BT4_SETUP_UI_LANGUAGE"),"en");
        if(getenv("BT4_SETUP_UI_MASK"))sInstallMask=atoi(getenv("BT4_SETUP_UI_MASK"));
        if(getenv("BT4_SETUP_PROGRESS_PREVIEW")){sPage=PAGE_RUN;sPackage=true;sPackagePercent=atoi(getenv("BT4_SETUP_PROGRESS_PREVIEW"));}
    }
    while (!sQuit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                sQuit = true;
            } else if (ev.type == SDL_EVENT_DROP_FILE && sPage == PAGE_PICK && ev.drop.data != NULL) {
                // (into the USA image's field, unless that one is filled already: then into the European one's)
                if (file_exists(sIso) && strcmp(sIso, ev.drop.data) != 0) {
                    SDL_strlcpy(sIsoEu, ev.drop.data, sizeof(sIsoEu));
                } else {
                    SDL_strlcpy(sIso, ev.drop.data, sizeof(sIso));
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(sDialogLock);
            if (sDialogReady) {
                SDL_strlcpy(sDialogFor ? sIsoEu : sIso, sDialogResult.c_str(), sDialogFor ? sizeof(sIsoEu) : sizeof(sIso));
                sDialogReady = false;
                if (sDialogFor && sPage == PAGE_DONE && sRelease && !Native_Running()) { // "Add languages...": only that step
                    sEuOnly = true;
                    start_install();
                }
            }
        }
        pump_install();
        if (sUseGl) {
            ImGui_ImplOpenGL3_NewFrame();
        } else {
            ImGui_ImplSDLGPU3_NewFrame();
        }
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        build_ui();
        ImGui::Render();
        ImDrawData *dd = ImGui::GetDrawData();
        if (sUseGl) {
            int pw = 0, ph = 0;
            SDL_GetWindowSizeInPixels(sWindow, &pw, &ph);
            glViewport(0, 0, pw, ph);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(dd);
            if (getenv("BT3_SETUP_SHOT") != NULL && ImGui::GetFrameCount() == 20 && pw > 0 && ph > 0) {
                // testing: the picture of the OpenGL window, <BT3_SETUP_SHOT>_gl.ppm (rows come bottom first)
                std::vector<unsigned char> px((size_t)pw * ph * 4);
                char name[512];
                glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                snprintf(name, sizeof(name), "%s_gl.ppm", getenv("BT3_SETUP_SHOT"));
                FILE *fp = fopen(name, "wb");
                if (fp != NULL) {
                    fprintf(fp, "P6\n%d %d\n255\n", pw, ph);
                    for (int y = ph - 1; y >= 0; y--) {
                        for (int x = 0; x < pw; x++) {
                            fwrite(&px[((size_t)y * pw + x) * 4], 1, 3, fp);
                        }
                    }
                    fclose(fp);
                }
            }
            SDL_GL_SwapWindow(sWindow);
            if (getenv("BT3_SETUP_QUIT") != NULL && ImGui::GetFrameCount() > 30) {
                sQuit = true; // testing: a few frames of the OpenGL window, then leave
            }
            continue;
        }
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sDevice);
        SDL_GPUTexture *swap = NULL;
        Uint32 w = 0, h = 0;
        if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, sWindow, &swap, &w, &h) && swap != NULL) {
            SDL_GPUColorTargetInfo target;
            ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
            SDL_zero(target);
            target.texture = swap;
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
            ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
            SDL_EndGPURenderPass(pass);
        }
        SDL_SubmitGPUCommandBuffer(cmd);
        if(getenv("BT4_RENDER_TEST")&&getenv("BT3_SETUP_SHOT")&&!w){int tw,th;SDL_GetWindowSizeInPixels(sWindow,&tw,&th);w=tw;h=th;}
        if (getenv("BT3_SETUP_SHOT") != NULL && w != 0 && ImGui::GetFrameCount() > 3) {
            int running = 0;
            for (size_t i = 0; i < sRows.size(); i++) {
                running += sRows[i].state != PENDING;
            }
            int key = (int)sPage * 100 + running;
            if (key != shotKey) {
                shotKey = key;
                shot(dd, w, h, shots++);
            }
            if (getenv("BT3_SETUP_QUIT") != NULL && (sPage == PAGE_DONE || sPage == PAGE_FAILED || (sProc == NULL && !sNative && shots > 0 && getenv("BT3_SETUP_ISO") == NULL))) {
                shot(dd, w, h, shots++);
                sQuit = true; // testing: leave once the result is on screen
            }
        }
    }
    if (sProc != NULL) {
        SDL_KillProcess(sProc, false);
        SDL_DestroyProcess(sProc);
    }
    Native_Join();
    if (sUseGl) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext(sGl);
        SDL_DestroyWindow(sWindow);
        SDL_Quit();
        return 0;
    }
    SDL_WaitForGPUIdle(sDevice);
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();
    SDL_ReleaseWindowFromGPUDevice(sDevice, sWindow);
    SDL_DestroyGPUDevice(sDevice);
    SDL_DestroyWindow(sWindow);
    SDL_Quit();
    return 0;
}
