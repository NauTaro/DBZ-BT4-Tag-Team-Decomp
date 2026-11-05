"""Managed-pack migration, using the production policy and process handoff.

Filesystem metadata and child creation are mocked; no multi-GB sparse files,
downloads, foreground windows or game installation are required.
"""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
from test_potara import function
ROOT=Path(__file__).resolve().parents[3]

class TextureUpdateTests(unittest.TestCase):
    def test_managed_pack_handoff_and_retry_guards(self):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('A C compiler is required')
        sdk=Path(os.environ.get('SDL3_DIR',''))
        if not (sdk/'include/SDL3/SDL.h').exists():self.skipTest('SDL3 headers are required')
        production=(ROOT/'port/src/gs/net_update.c').read_text(encoding='utf8')
        policy=(ROOT/'port/include/bt4_texture_update.h').read_text(encoding='utf8')
        code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <SDL3/SDL.h>
#include "bt4_texture_pack.h"
static int oldPack=1,newPack,setup=1,skip,testing,replay,demo,spawned,fail;
static const char *base="C:/BT4 with spaces/";
static bool path_info(const char *p,SDL_PathInfo *info){
 SDL_PathInfo v;memset(&v,0,sizeof(v));v.type=SDL_PATHTYPE_FILE;
 if(strstr(p,BT4_TEXTURE_ARCHIVE)){if(!newPack)return false;v.size=newPack==1?BT4_TEXTURE_ARCHIVE_BYTES:17;}
 else if(strstr(p,BT4_TEXTURE_LEGACY_ARCHIVE)){if(!oldPack)return false;v.size=oldPack==1?BT4_TEXTURE_LEGACY_BYTES:19;}
 else if(strstr(p,"BT4decomp-setup")){if(!setup)return false;}
 else return false;
 if(info)*info=v;return true;
}
static const char *base_path(void){return base;}
static const char *env(const char *key){
 return (!strcmp(key,"BT4_TEXTURE_UPDATE_SKIP")&&skip)||(!strcmp(key,"BT4_RENDER_TEST")&&testing)||
 (!strcmp(key,"BT3_REPLAY")&&replay)||(!strcmp(key,"BT3_DEMO")&&demo)?"1":NULL;
}
static int format(char *p,size_t n,const char *f,...){va_list a;va_start(a,f);int r=vsnprintf(p,n,f,a);va_end(a);return r;}
static SDL_PropertiesID make_props(void){return 1;}
static bool pointer(SDL_PropertiesID p,const char *key,void *v){
 assert(p==1);const char *const *args=(const char *const *)v;
 assert(strstr(args[0],"C:/BT4 with spaces/BT4decomp-setup"));
 assert(!strcmp(args[1],"--update-textures")&&!strcmp(args[2],"--ui-language"));
 assert(!strcmp(args[3],"2")&&!args[4]);return true;
}
static bool string(SDL_PropertiesID p,const char *key,const char *v){assert(p==1&&!strcmp(v,base));return true;}
static bool number(SDL_PropertiesID p,const char *key,Sint64 v){return true;}
static bool boolean(SDL_PropertiesID p,const char *key,bool v){assert(v);return true;}
static SDL_Process *spawn(SDL_PropertiesID p){++spawned;return fail?NULL:(SDL_Process *)1;}
static void destroy_props(SDL_PropertiesID p){}
static void destroy_process(SDL_Process *p){assert(p==(SDL_Process *)1);}
static const char *error(void){return "test failure";}
#define SDL_GetPathInfo path_info
#define SDL_GetBasePath base_path
#define SDL_getenv env
#define SDL_snprintf format
#define SDL_CreateProperties make_props
#define SDL_SetPointerProperty pointer
#define SDL_SetStringProperty string
#define SDL_SetNumberProperty number
#define SDL_SetBooleanProperty boolean
#define SDL_CreateProcessWithProperties spawn
#define SDL_DestroyProperties destroy_props
#define SDL_DestroyProcess destroy_process
#define SDL_GetError error
'''+policy+'\n'+function(production,'int Update_StartTextureMaintenance(int locale) {')+r'''
int main(void){
 assert(Bt4Texture_NeedsUpdate(base));
 assert(Update_StartTextureMaintenance(2)&&spawned==1);
 newPack=1;assert(!Bt4Texture_NeedsUpdate(base));assert(!Update_StartTextureMaintenance(2));
 newPack=2;assert(Bt4Texture_NeedsUpdate(base));newPack=0;
 oldPack=2;assert(!Bt4Texture_NeedsUpdate(base));oldPack=0;assert(!Bt4Texture_NeedsUpdate(base));oldPack=1;
 skip=1;assert(!Update_StartTextureMaintenance(2));skip=0;
 testing=1;assert(!Update_StartTextureMaintenance(2));testing=0;
 replay=1;assert(!Update_StartTextureMaintenance(2));replay=0;
 demo=1;assert(!Update_StartTextureMaintenance(2));demo=0;
 setup=0;assert(!Update_StartTextureMaintenance(2));setup=1;
 assert(spawned==1);fail=1;assert(!Update_StartTextureMaintenance(2)&&spawned==2);
 puts("Texture migration: installed/current/custom/missing packs, exact args, failure and launch guards PASS");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'probe.c').write_text(code,encoding='utf8');exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99','-I'+str(sdk/'include'),'-I'+str(ROOT/'port/include'),str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_only_exact_legacy_archive_is_retired(self):
        cc=os.environ.get('CXX') or shutil.which('g++') or shutil.which('c++')
        if not cc:self.skipTest('A C++ compiler is required')
        source=(ROOT/'port/setup/native.cpp').read_text(encoding='utf8')
        code=r'''
#include <string>
#include <cassert>
#include <cstdio>
#include "bt4_texture_pack.h"
typedef unsigned long long Uint64;
enum {SDL_PATHTYPE_FILE=1};struct SDL_PathInfo {int type;Uint64 size;};
static bool present=true;static int type=1,removed;static Uint64 size=BT4_TEXTURE_LEGACY_BYTES;
static std::string digest=BT4_TEXTURE_LEGACY_SHA256;
static bool SDL_GetPathInfo(const char *p,SDL_PathInfo *out){if(!present)return false;out->type=type;out->size=size;return true;}
static std::string sha256_file(const std::string &p){return digest;}
static bool SDL_RemovePath(const char *p){++removed;return true;}
static void say(const std::string &s){}
'''+function(source,'void retire_legacy_texture_pack(const std::string &root) {')+r'''
int main(){
 present=false;retire_legacy_texture_pack("root/");assert(!removed);present=true;
 type=2;retire_legacy_texture_pack("root/");assert(!removed);type=1;
 size=123;retire_legacy_texture_pack("root/");assert(!removed);size=BT4_TEXTURE_LEGACY_BYTES;
 digest="customized";retire_legacy_texture_pack("root/");assert(!removed);
 digest=BT4_TEXTURE_LEGACY_SHA256;retire_legacy_texture_pack("root/");assert(removed==1);
 puts("Texture replacement: old pack removal requires the original filename, size and SHA-256 PASS");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'probe.cpp').write_text(code,encoding='utf8');exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=c++17','-I'+str(ROOT/'port/include'),str(d/'probe.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__=='__main__':unittest.main()
