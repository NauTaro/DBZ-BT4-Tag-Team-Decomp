/* Full disc profiles. Selection precedes game data loading; no live data swap. */
#include "game_profile.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
extern void Port_SettingText(const char *,const char *,char *,int);
extern void Port_SettingTextSave(const char *,const char *);
extern void Port_SettingsWrite(void);
static int sEnglish,sExternal,sSkip,sReady[2];
static int sMenuActive,sMenuChoice,sMenuAlpha;
void Port_GameProfileMenuSet(int active,int choice,int alpha){
    sMenuActive=active!=0;sMenuChoice=choice!=0;sMenuAlpha=alpha<0?0:alpha>128?128:alpha;
}
int Port_GameProfileMenuActive(void){return sMenuActive;}
int Port_GameProfileMenuChoice(void){return sMenuChoice;}
int Port_GameProfileMenuAlpha(void){return sMenuAlpha;}
static char sRoots[2][1600],sError[192];
/* Conversion is a separate low-priority hidden process. Starting a room never
 * scans or converts the music library; missing cache entries use original ADX. */
static void music_prepare(void){
#ifdef _WIN32
    char complete[128],tool[1800],command[4000];WCHAR wideTool[1800],wideCommand[4000];
    FILE *f;const char *base=SDL_GetBasePath();STARTUPINFOW si;PROCESS_INFORMATION pi;
    if(getenv("BT4_RENDER_TEST")||getenv("BT3_NOSOUND")||getenv("BT4_MUSIC_NO_CACHE"))return;
    snprintf(complete,sizeof(complete),"tbo/music/v1/.complete-%s",sEnglish?"en":"es");
    f=fopen(complete,"rb");if(f){fclose(f);return;}
    snprintf(tool,sizeof(tool),"%stbo/tools/BT4-Music.exe",base?base:"");f=fopen(tool,"rb");if(!f)return;fclose(f);
    _mkdir("tbo");_mkdir("tbo/music");_mkdir("tbo/music/v1");
    snprintf(command,sizeof(command),"\"%s\" --library \"%s\" \"tbo/music/v1\" %s",tool,sRoots[sEnglish],sEnglish?"en":"es");
    if(!MultiByteToWideChar(CP_UTF8,0,tool,-1,wideTool,1800)||!MultiByteToWideChar(CP_UTF8,0,command,-1,wideCommand,4000))return;
    memset(&si,0,sizeof(si));si.cb=sizeof(si);memset(&pi,0,sizeof(pi));
    if(CreateProcessW(wideTool,wideCommand,NULL,NULL,FALSE,CREATE_NO_WINDOW|IDLE_PRIORITY_CLASS,NULL,NULL,&si,&pi)){
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    }
#endif
}
/* SDL is a DLL with its own C runtime on Windows. Change this executable's
 * environment so all native and recompiled file readers see the same root. */
static void profile_env_set(const char *name,const char *value){
#ifdef _WIN32
    _putenv_s(name,value?value:"");
#else
    if(value)setenv(name,value,1);else unsetenv(name);
#endif
}
static int readable(const char *root,const char *relative){
    char path[1800];FILE *f;
    if(SDL_snprintf(path,sizeof(path),"%s/%s",root,relative)>=(int)sizeof(path))return 0;
    f=fopen(path,"rb");if(!f)return 0;fclose(f);return 1;
}
static int base_language(const char *root){
    char path[1800],line[160]={0};FILE *f;
    SDL_snprintf(path,sizeof(path),"%s/.bt4-language",root);f=fopen(path,"rb");
    if(f){fgets(line,sizeof(line),f);fclose(f);return !strncmp(line,"en",2);}
    SDL_snprintf(path,sizeof(path),"%s/.installed-bt4-b14rev2",root);f=fopen(path,"rb");
    if(f){fgets(line,sizeof(line),f);fclose(f);}
    return strstr(line,"ENG")!=NULL;
}
void Port_GameProfileInit(void){
    const char *override=getenv("BT3_DATA"),*base=SDL_GetBasePath();char root[1600],saved[16];int installed,i;
    sExternal=override&&override[0];sSkip=getenv("BT4_PROFILE_SKIP_ONCE")!=NULL;
    profile_env_set("BT4_PROFILE_SKIP_ONCE",NULL);
    SDL_snprintf(root,sizeof(root),"%s",sExternal?override:"gamedata");
    installed=base_language(root);
    for(i=0;i<2;i++){
        if(i==installed)SDL_snprintf(sRoots[i],sizeof(sRoots[i]),"%s",root);
        else SDL_snprintf(sRoots[i],sizeof(sRoots[i]),"%s/languages/%s",root,i?"en":"es");
        sReady[i]=readable(sRoots[i],"disc/SLUS_219.78")&&readable(sRoots[i],"disc/BIN/DBZP.BIN")&&
            (i==installed||readable(sRoots[i],".installed-bt4-b14rev2"));
    }
    {char file[1800];FILE *f;int mask=3;
     SDL_snprintf(file,sizeof(file),"%stbo/languages.ini",base?base:"");f=fopen(file,"rb");
     if(f){if(fscanf(f,"%d",&mask)!=1||mask<1||mask>3)mask=3;fclose(f);}
     for(i=0;i<2;++i)if(!(mask&(1<<i)))sReady[i]=0;}
    Port_SettingText("game_language",installed?"en":"es",saved,sizeof(saved));
    sEnglish=sExternal?installed:!strcmp(saved,"en");
    if(!sReady[sEnglish])sEnglish=sReady[installed]?installed:!installed;
    profile_env_set("BT3_DATA",sRoots[sEnglish]);
    if(!getenv("BT3_MENU_LANG"))profile_env_set("BT3_MENU_LANG","en");
    fprintf(stderr,"bt4: game profile=%s data=%s\n",sEnglish?"English":"Spanish",sRoots[sEnglish]);
    music_prepare();
}
int Port_GameProfileEnglish(void){return sEnglish;}
int Port_GameProfileAvailable(int english){return sReady[english!=0];}
int Port_GameProfilePrompt(void){
    extern int Port_NetSession(void);
    if(!sReady[0]||!sReady[1])return 0;
    /* A session restores the boot snapshot in-process. Its neutral network
     * input must never reopen the language dialog and wait for a choice. */
    if(Port_NetSession())return 0;
    if(sSkip)return 0;
    if(getenv("BT4_PROFILE_PICKER_TEST"))return 1;
    return !sExternal&&!sSkip&&!getenv("BT3_DEMO")&&!(getenv("BT3_GS")&&!strcmp(getenv("BT3_GS"),"none"))&&!getenv("BT3_PAD_PLAY")&&!getenv("BT3_PAD_TABLE")&&!getenv("BT3_SYNCTEST")&&!getenv("BT3_NET_SESSION");
}
const char *Port_GameProfileError(void){return sError;}
int Port_GameProfileChoose(int english){
    char previous[16];english=english!=0;sError[0]=0;
    if(!sReady[english]){
        SDL_snprintf(sError,sizeof(sError),"Install the %s ISO once using the launcher.",english?"English":"Spanish");return 0;
    }
    Port_SettingText("game_language",sEnglish?"en":"es",previous,sizeof(previous));
    Port_SettingTextSave("game_language",english?"en":"es");
    /* 'en' means the disc's native text; do not overlay a translation pack. */
    Port_SettingTextSave("menu_lang","en");Port_SettingsWrite();
    if(english==sEnglish)return 1;
    if(getenv("BT4_PROFILE_NO_SPAWN_TEST"))return 2;
#ifdef _WIN32
    WCHAR executable[32768];STARTUPINFOW startup;PROCESS_INFORMATION process;
    memset(&startup,0,sizeof(startup));startup.cb=sizeof(startup);memset(&process,0,sizeof(process));
    LPWCH original=GetEnvironmentStringsW(),item;WCHAR *child=NULL,*write;size_t count=64;
    if(original){
        for(item=original;*item;item+=wcslen(item)+1)count+=wcslen(item)+1;
        child=(WCHAR *)malloc(count*sizeof(WCHAR));write=child;
        if(child){
            for(item=original;*item;item+=wcslen(item)+1){
                if(!_wcsnicmp(item,L"BT3_DATA=",9)||!_wcsnicmp(item,L"BT4_PROFILE_SKIP_ONCE=",22))continue;
                size_t n=wcslen(item)+1;memcpy(write,item,n*sizeof(WCHAR));write+=n;
            }
            wcscpy(write,L"BT4_PROFILE_SKIP_ONCE=1");write+=wcslen(write)+1;*write=0;
        }
        FreeEnvironmentStringsW(original);
    }
    int launched=child&&GetModuleFileNameW(NULL,executable,32768)&&CreateProcessW(executable,NULL,NULL,NULL,FALSE,
       CREATE_UNICODE_ENVIRONMENT|(getenv("BT4_RENDER_TEST")?CREATE_NO_WINDOW:0),child,NULL,&startup,&process);
    free(child);
    if(launched){
        CloseHandle(process.hThread);CloseHandle(process.hProcess);exit(0);
    }
#else
    {const char *base=SDL_GetBasePath();char exe[1800];SDL_setenv_unsafe("BT4_PROFILE_SKIP_ONCE","1",1);SDL_unsetenv_unsafe("BT3_DATA");SDL_snprintf(exe,sizeof(exe),"%sBT4decomp",base?base:"");execl(exe,exe,(char *)NULL);}
#endif
    profile_env_set("BT3_DATA",sRoots[sEnglish]);profile_env_set("BT4_PROFILE_SKIP_ONCE",NULL);
    Port_SettingTextSave("game_language",previous);Port_SettingsWrite();
    SDL_snprintf(sError,sizeof(sError),"Could not restart the game. Please close it and try again.");return 0;
}
