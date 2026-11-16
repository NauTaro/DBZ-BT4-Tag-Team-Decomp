"""Execute the shipped Lua HUDs and native camera/prompt paths across both modes."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
from test_potara import function

ROOT = Path(__file__).resolve().parents[3]
HIDDEN = {'creationflags': 0x08000000} if os.name == 'nt' else {}


class HudTeamIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cc = shutil.which('gcc') or shutil.which('cc')
        if not cls.cc:
            raise unittest.SkipTest('C compiler required')
        cls.temp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.temp.name)
        lua = ROOT/'port/third_party/lua'
        runner = cls.out/'runner.c'
        runner.write_text(r'''
#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
int main(int argc,char **argv) {
    lua_State *L=luaL_newstate();
    const luaL_Reg libs[]={{"_G",luaopen_base},{"math",luaopen_math},
        {"table",luaopen_table},{"string",luaopen_string},{NULL,NULL}};
    for(int i=0;libs[i].name;++i){luaL_requiref(L,libs[i].name,libs[i].func,1);lua_pop(L,1);}
    for(int i=2;i<argc;++i){lua_pushstring(L,argv[i]);lua_setglobal(L,i==2?"hud_path":"skills_path");}
    int status=luaL_loadfile(L,argv[1]);
    if(!status)status=lua_pcall(L,0,0,0);
    if(status)fprintf(stderr,"%s\n",lua_tostring(L,-1));
    lua_close(L);return status?1:0;
}
''', encoding='utf-8')
        cls.runner = cls.out/('runner.exe' if os.name == 'nt' else 'runner')
        args = [cls.cc, '-O1', '-I'+str(lua), str(runner),
                *map(str, lua.glob('*.c')), '-o', str(cls.runner), '-lm']
        p = subprocess.run(args, capture_output=True, text=True, **HIDDEN)
        if p.returncode:
            cls.temp.cleanup()
            raise RuntimeError(p.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def lua(self, source):
        script = self.out/'probe.lua'
        script.write_text(source, encoding='utf-8')
        p = subprocess.run([str(self.runner), str(script),
            str(ROOT/'port/examples/mods/modern_hud/mod.lua'),
            str(ROOT/'port/examples/mods/skill_list/mod.lua')],
            capture_output=True, text=True, timeout=15, **HIDDEN)
        self.assertEqual(p.returncode, 0, p.stdout+p.stderr)

    def c(self, source):
        script, exe = self.out/'native.c', self.out/('native.exe' if os.name=='nt' else 'native')
        script.write_text(source, encoding='utf-8')
        p = subprocess.run([self.cc, '-w', str(script), '-o', str(exe)], capture_output=True, text=True, **HIDDEN)
        self.assertEqual(p.returncode, 0, p.stderr)
        p = subprocess.run([str(exe)], capture_output=True, text=True, timeout=5, **HIDDEN)
        self.assertEqual(p.returncode, 0, p.stdout+p.stderr)

    def test_all_reserves_visible_after_switch_both_aspects_and_sides(self):
        self.lua(r'''
local locale, settings = "en", {opacity=100,timer=true,combo=true}
bt3={setting=function(k)return settings[k]end,locale=function()return locale end,label=function()return nil end}
for _,language in ipairs({"es","en","pt-BR"}) do
locale=language
for _,aspect in ipairs({4/3,16/9}) do for _,swapped in ipairs({false,true}) do
    local mod=dofile(hud_path)
    local calls, hidden={},{}
    local hud=setmetatable({h=448,w=448*aspect},{__index=function(_,k)
        return function(...)calls[#calls+1]={kind=k,args={...}}end end})
    hud.hide_game=function(k)hidden[k]=true end
    local function fighter()
        local f={active=1,chara=0,hp=40000,hp_max=40000,ki=100000,ki_max=100000,
            blast=200000,blast_max=500000,max_power=0,hud_hits=0,hud_damage=-1,
            hit_seq=0,message_seq=0,switch_gauge=50000,next=3,members={}}
        for i=1,5 do f.members[i]={present=true,hp=i==5 and 0 or 40000,hp_max=40000}end
        return f
    end
    local game={time=1,battle=true,hud_shown=true,swapped=swapped,fighters={fighter(),fighter()}}
    mod.on_hud(hud,game);game.time=2;calls={};mod.on_hud(hud,game)
    for _,active in ipairs({1,3,5}) do
        game.fighters[1].active=active;game.fighters[2].active=active
        calls={};game.time=game.time+1;mod.on_hud(hud,game)
        local seen,count={{},{}},0
        for _,c in ipairs(calls)do if c.kind=="portrait" and c.args[6].member then
            local a=c.args;local member=a[6].member
            assert(member~=active and not seen[a[1]][member]);seen[a[1]][member]=true;count=count+1
            assert(a[2]>=0 and a[2]+a[4]<=hud.w and a[3]>100*448/940 and a[3]+a[5]<188*448/940)
            -- Reserves stay outside the active portrait's 0..230 layout window.
            local left=(a[1]==1)~=swapped
            assert(left and a[2]>230*448/940 or not left and a[2]+a[4]<hud.w-230*448/940)
        end end
        assert(count==8 and hidden.team and hidden.gauges)
    end
end end end
''')

    def test_native_prompts_take_priority_even_with_always_shown_skills(self):
        self.lua(r'''
local settings={size=100,always=true,buttons=true}
bt3={setting=function(k)return settings[k]end,locale=function()return "en"end,label=function()return nil end}
for mode=1,3 do
    local skills=dofile(skills_path)
    local draws, suppress=0,false
    local hud=setmetatable({h=448,w=796},{__index=function()return function()draws=draws+1 end end})
    hud.hide_game=function(k)if k=="prompt"then suppress=true end end
    local f={cpu=false,charge_held=true,ki=100000,prompt_shown=true,
        moves={{name="Kamehameha",kind="special",cost=60000,usable=true,button="triangle",direction=""}}}
    local game={hud_shown=true,fighters={f},me=1,split=mode==3}
    if mode>1 then game.tag_team={spectating={false,false}}end
    for i=1,20 do skills.on_hud(hud,game)end
    assert(draws==0 and not suppress,"native commands covered")
    f.prompt_shown=false;skills.on_hud(hud,game)
    assert(draws>0 and not suppress,"skills did not return after command")
    f.prompt_shown=true;draws=0;skills.on_hud(hud,game)
    assert(draws==0 and not suppress,"animated list lingered over command")
end
''')

    def test_second_split_hud_is_mirrored_and_leaves_timer_clear(self):
        self.lua(r'''
bt3={setting=function(k)return k=="opacity" and 100 or true end,
    locale=function()return "es"end,label=function()return nil end}
for _,aspect in ipairs({4/3,16/9})do for _,tagMode in ipairs({false,true})do
    local mod=dofile(hud_path)
    local calls={}
    local hud=setmetatable({h=448,w=448*aspect},{__index=function(_,k)
        return function(...)calls[#calls+1]={kind=k,args={...}}end end})
    local function fighter(actor)
        return {actor=actor,found=true,active=1,team=actor==1 and 0 or 1,chara=0,hp=40000,hp_max=40000,
            ki=100000,ki_max=100000,blast=100000,blast_max=500000,max_power=0,
            hud_hits=0,hud_damage=-1,hit_seq=0,message_seq=0}
    end
    local f={fighter(tagMode and 1 or nil),fighter(tagMode and 2 or nil)}
    local game={time=1,battle=true,hud_shown=true,split=true,presentation_split=true,
        fighters=f,timer={value=300,clock=true}}
    if tagMode then
        game.tag_team={view=1,views={1,2},actors={fighter(1),fighter(2)}}
    end
    mod.on_hud(hud,game);calls={};game.time=2;mod.on_hud(hud,game)
    local portraits=0
    for _,c in ipairs(calls)do if c.kind=="portrait" and c.args[6].face then
        portraits=portraits+1
        local opts=c.args[6];local window=opts.window
        assert(window.x0<window.x1 and window.x0>=0 and window.x1<=hud.w)
        if portraits==1 then assert(not opts.flip and window.x1<hud.w/2-40)
        elseif portraits==2 then assert(opts.flip and window.x0>hud.w/2+40)end
    end end
    assert(portraits==2)
end end
''')

    def test_native_prompt_query_includes_fades_and_null_lifecycle(self):
        source = (ROOT/'src/battle/hud_e_b.c').read_text(encoding='utf-8')
        self.c(r'''
#include <assert.h>
#include <stddef.h>
typedef int s32;
struct Ramp{float value;};
static struct {int btnState[2];struct {int on[2];}cmd;struct Ramp cmdRamp[2],nameFade[2];}work,*gHudPrompt;
''' + function(source, 's32 Port_HudPromptVisible(') + r'''
int main(){assert(!Port_HudPromptVisible(0));gHudPrompt=&work;
assert(!Port_HudPromptVisible(-1)&&!Port_HudPromptVisible(2)&&!Port_HudPromptVisible(0));
work.btnState[0]=1;assert(Port_HudPromptVisible(0)&&!Port_HudPromptVisible(1));
work.btnState[0]=-1;work.cmd.on[1]=1;assert(Port_HudPromptVisible(1));
work.cmd.on[1]=0;work.cmdRamp[1].value=.1f;assert(Port_HudPromptVisible(1));
work.cmdRamp[1].value=0;work.nameFade[1].value=.1f;assert(Port_HudPromptVisible(1));
work.nameFade[1].value=0;assert(!Port_HudPromptVisible(1));}
''')

    def test_split_5v5_keeps_two_main_portraits_without_fixed_ally_rosters(self):
        self.lua(r'''
bt3={setting=function(k)return k=="opacity" and 100 or true end,
    locale=function()return "en"end,label=function()return nil end}
local mod=dofile(hud_path)
local portraits={}
local hud=setmetatable({h=448,w=796},{__index=function()return function()end end})
hud.portrait=function(...)portraits[#portraits+1]={...}end
local actors={}
for i=1,10 do actors[i]={actor=i,found=true,team=i<=5 and 0 or 1,
    active=1,chara=i,hp=40000,hp_max=40000,ki=100000,ki_max=100000,
    blast=100000,blast_max=500000,max_power=0,hud_hits=0,hud_damage=-1,hit_seq=0,message_seq=0}end
local game={time=1,battle=true,hud_shown=true,split=true,
    tag_team={view=1,views={1,6},actors=actors},fighters={{},{}}}
mod.on_hud(hud,game);portraits={};game.time=2;mod.on_hud(hud,game)
assert(#portraits==2,"a fixed ally roster still occupies the split view")
assert(portraits[1][6].actor==1 and portraits[2][6].actor==6)
''')

    def test_floating_allies_reuse_modern_portrait_health_and_ki_widget(self):
        self.lua(r'''
bt3={setting=function(k)return k=="opacity" and 100 or true end,
    locale=function()return "en"end,label=function()return nil end}
local mod=dofile(hud_path)
local calls,portraits={},{}
local hud=setmetatable({h=448,w=796},{__index=function(_,k)return function(...)
    calls[#calls+1]={kind=k,args={...}}
end end})
hud.portrait=function(...)portraits[#portraits+1]={...}end
local actors={}
for i=1,10 do actors[i]={actor=i,found=true,team=i<=5 and 0 or 1,
    active=1,chara=i,hp=40000,hp_max=40000,ki=100000,ki_max=100000,
    blast=100000,blast_max=500000,max_power=0,hud_hits=0,hud_damage=-1,hit_seq=0,message_seq=0}end
actors[4].absorbed=true
local game={time=1,battle=true,hud_shown=true,split=true,
    tag_team={view=1,views={1,6},actors=actors,floating_allies={
        {{actor=2,x=.2,y=.4},{actor=4,x=.4,y=.3}},{{actor=3,x=.8,y=.5}}}},fighters={{},{}}}
mod.on_hud(hud,game);portraits={};calls={};game.time=2;mod.on_hud(hud,game)
assert(#portraits==4,"two main portraits and two visible floating widgets expected")
assert(portraits[3][6].actor==2 and portraits[4][6].actor==3)
for i=3,4 do
    local a=portraits[i];local opts=a[6]
    assert(opts.face and opts.fade and opts.window and a[4]<30,"floating character artwork lost")
end
local bands=0
local anchored,ended=0,0
for _,c in ipairs(calls)do if c.kind=="band"then bands=bands+1 end end
for _,c in ipairs(calls)do
    if c.kind=="actor_anchor"then anchored=anchored+1;assert(c.args[1]==anchored and c.args[2]==anchored+1)end
    if c.kind=="unanchor"then ended=ended+1 end
end
assert(bands>0,"the Modern HUD health/ki strips must retain their native curved layout")
assert(anchored==2 and ended==2,"each floating widget must follow its own presented camera")
''')

    def test_compact_cost_numerals_follow_the_measured_technique_name(self):
        self.lua(r'''
local settings={size=100,always=true,buttons=true}
bt3={setting=function(k)return settings[k]end,locale=function()return "es"end,label=function()return nil end}
for _,name in ipairs({"Kaioken","Teletransportación","Instant Transmission","Transmissão instantânea"})do
    local skills=dofile(skills_path)
    local calls={}
    local hud=setmetatable({h=448,w=796},{__index=function(_,k)
        return function(...)calls[#calls+1]={kind=k,args={...}}end end})
    local game={hud_shown=true,me=1,tag_team={},fighters={{cpu=false,
        moves={{name=name,kind="skill",cost=300000,usable=true,button="circle",direction=""}}}}}
    skills.on_hud(hud,game)
    local last,found
    for _,c in ipairs(calls)do if c.kind=="text"then
        local a=c.args
        if a[3]=="3"then
            assert(last==name and a[7].before and not a[7].after and a[1]==6*.72)
            assert(a[5]==24*.72 and a[7].outline==0x0B1118FF and a[7].lean==.12)
            found=true
        end
        last=a[3]
    end end
    assert(found)
end
''')

    def test_stock_mod_upgrade_preserves_edited_files_and_unknown_mods(self):
        source = (ROOT/'port/src/gs/mods.c').read_text(encoding='utf-8')
        includes = ''.join('#include "'+(ROOT/'port/src/gs'/name).as_posix()+'"\n' for name in (
            'bundled_skill_list_0394.h','bundled_mods_r1.h','bundled_modern_hud_r2.h',
            'bundled_mods_r58_tag_hotfix2.h','bundled_modern_hud_hotfix3.h'))
        self.c(r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int missing;
static char *read_file(const char *path,long *n){
    if(missing)return NULL;
    char *s=malloc(4);memcpy(s,"new",4);*n=3;return s;
}
''' + includes + function(source, 'static void upgrade_bundled_text(') + r'''
static void check(const char *name,const unsigned char *old,long size){
    char *text=malloc(size+2);memcpy(text,old,size);text[size]=0;long n=size;
    missing=1;upgrade_bundled_text("bundled",name,&text,&n);
    assert(n==size&&!memcmp(text,old,size));missing=0;
    /* Even a one-byte user edit must not be replaced. */
    text[0]^=1;upgrade_bundled_text("bundled",name,&text,&n);assert(n==size&&text[0]!=(char)old[0]);
    text[0]^=1;upgrade_bundled_text("bundled","my_hud",&text,&n);assert(n==size&&!memcmp(text,old,size));
    upgrade_bundled_text("bundled",name,&text,&n);assert(n==3&&!memcmp(text,"new",3));free(text);
}
int main(){
check("modern_hud",kPublicR58_modern_hud,sizeof(kPublicR58_modern_hud)-1);
check("modern_hud",kTagHotfix2_modern_hud,sizeof(kTagHotfix2_modern_hud)-1);
check("modern_hud",kTagHotfix3ModernHud,sizeof(kTagHotfix3ModernHud)-1);
check("skill_list",kPublicR58_skill_list,sizeof(kPublicR58_skill_list)-1);
check("skill_list",kTagHotfix2_skill_list,sizeof(kTagHotfix2_skill_list)-1);
}
''')

    def test_tag_split_camera_uses_view_index_for_any_of_ten_fighters(self):
        source = (ROOT/'src/battle/btl_cam.c').read_text(encoding='utf-8')
        self.c(r'''
#include <assert.h>
typedef int s32;
#define PORT 1
#define BT4_TAG_TEAM_EXPANDED 1
typedef struct {float x,y,z,w;} Vec4;
typedef struct {Vec4 pos,rot;int priority;} BtlCamView;
static BtlCamView views[2];static struct{BtlCamView *cur;}work,*gBtlCam=&work;
static int actor[2],expected,tag=1,split=1;
int TagBattle_Active(){return tag;}int Battle_IsSplitScreen(){return split;}
int TagBattle_SplitViewActor(int side){return actor[side];}
int BattleSide_GetObjId(int a){return 100+a;}
void BtlCam_SetLayout(int a,int b){}
void BtlCharApi_GetCamPose(int id,Vec4*p,Vec4*r){p->x=id;}
int BtlCharApi_HasCamPriority(int id){return id;}
void BtlCam_PortMods(BtlCamView*v,int side,int id){assert(side==expected&&side<2);assert(id==100+actor[side]);}
void BtlCam_SplitSpectatorOrbit(BtlCamView*v,int side,int id){assert(side==expected);}
void BtlCam_BuildView(BtlCamView*v,int n){}
''' + function(source, 'void BtlCam_UpdateView(') + r'''
int main(){for(int a=0;a<10;++a)for(int b=0;b<10;++b){
actor[0]=a;actor[1]=b;for(expected=0;expected<2;++expected){gBtlCam->cur=&views[expected];BtlCam_UpdateView(expected);}
}return 0;}
''')
