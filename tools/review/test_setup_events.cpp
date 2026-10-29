// Headless event/UI-state regression tests. No SDL window, game or network.
#define main SetupWindowEntry
#include "../../port/setup/setup.cpp"
#undef main
#include <cassert>

static double percent(){
    double done=0,total=0;
    for(auto &row:sRows){double w=SetupStepWeight(row.title.c_str());total+=w;
        done+=w*((row.state==OK||row.state==SKIPPED)?1:row.progress.fraction);}
    return SetupPercent(done,total,!sLauncher.empty());
}
int main(){
    SDL_Init(0);sRelease=true;sInstallMask=1;reset_rows();
    event("@step 1 4 Disc image");event("@ok verified");
    event("@step 2 4 Game data");event("@activity extract");
    event("@progress 1000000 4000000000 4 25000");double early=percent();
    event("@progress 2000000000 4000000000 12000 25000");
    assert(percent()>early && current()->progress.files==12000);
    event("@progress 4000000000 4000000000 25000 25000");event("@ok extracted");
    event("@step 3 4 Native data tables");event("@ok prepared");
    event("@step 4 4 Self-test");assert(percent()<100);
    event("@fail");event("@stopped test failed");assert(percent()<100 && sLauncher.empty());
    sError.clear();reset_rows();sInstallMask=3;
    // Adding a language to an existing install starts at step 1 of 2.
    event("@step 1 2 Language package");event("@package 65");double cached=percent();
    event("@package 0");assert(percent()==cached); // bad cache falls back to download
    event("@package 100");event("@ok unpacked");
    event("@step 2 2 Self-test");assert(!sPackage && sActivity.empty() && percent()<100);
    event("@ok tested");assert(percent()<100);
    event("@activity finalize");assert(percent()<100);
    event("@done fixture/BT4decomp");assert(percent()==100);
    sLauncher.clear();reset_rows();assert(percent()==0);
    printf("PASS: extraction events, phase changes, cached language, failure/retry and final 100%% gate\n");
    SDL_Quit();return 0;
}
