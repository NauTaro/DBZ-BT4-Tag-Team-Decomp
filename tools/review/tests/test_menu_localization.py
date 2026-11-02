"""Regression coverage for Portuguese catalogs and literal caption wrapping."""
from pathlib import Path
import importlib.util,json,os,shutil,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

class MenuLocalizationTests(unittest.TestCase):
    def test_release_contains_native_bindings_and_current_portuguese_catalog(self):
        sys.path.insert(0,str(ROOT/'port/tools'))
        try:
            spec=importlib.util.spec_from_file_location('bt4_test_package',ROOT/'port/tools/package.py')
            package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)
            with tempfile.TemporaryDirectory() as directory:
                out=Path(directory);package.take_translations(out)
                for relative in ['labels/pt-BR-native.tsv','port/pt-BR.json']:
                    self.assertEqual((out/'translations'/relative).read_bytes(),(ROOT/'port/translations'/relative).read_bytes())
        finally:sys.path.pop(0)

    def compile_run(self,code,*args):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory);src=path/'probe.c';exe=path/('probe.exe' if os.name=='nt' else 'probe')
            src.write_text(code,encoding='utf8')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            dead='-Wl,-dead_strip' if os.sys.platform=='darwin' else '-Wl,--gc-sections'
            p=subprocess.run([cc,'-std=gnu99','-O1','-ffunction-sections','-fdata-sections',str(src),'-o',str(exe),dead,'-lpthread','-lm'],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)

    def test_actual_catalog_parser_and_legacy_fragment_rejection(self):
        path=(ROOT/'port/src/gs/menu_translation.c').as_posix()
        code='#include <assert.h>\n#include "'+path+'"\n'+r'''
int Port_NetSession(void){return 0;}
int Port_Setting(const char *key,int value){return value;}
void Port_SettingText(const char *key,const char *value,char *out,int size){snprintf(out,size,"%s",value);}
void Port_SettingTextSave(const char *key,const char *value){}
void Port_SettingSave(const char *key,int value){}
void Port_SettingsWrite(void){}
int MenuText_FirstFontId(int setting){return 0;}
int UiText_Load(const char *dir,const char *locale){return 0;}
void UiText_ExtractOnStartup(const char *dir){}
int main(int argc,char **argv){
 TrPortCatalog cat;
 const char *legacy="{\"schema_version\":1,\"locale\":\"pt-BR\",\"labels\":{\"tbo/wrap-fix/1\":{\"original\":\"Quick Match\",\"text\":\"WWWWWW\"}}}";
 assert(tr_port_parse(legacy,strlen(legacy),"pt-BR",&cat));assert(!cat.lines);
 assert(argc==2 && tr_port_read_path(argv[1],"pt-BR",&cat));assert(cat.lines && cat.lines->count>500);
 int found=0;
 for(int i=0;i<cat.lines->count;i++){
  TrLine *line=&cat.lines->line[i];assert(!strstr(line->text,"WWWW"));
  if(!strcmp(line->original,"Quick Match")){assert(!strcmp(line->text,"Partida rápida"));found=1;}
 }
 assert(found);tr_lines_free(cat.lines);return 0;
}
'''
        self.compile_run(code,ROOT/'port/translations/port/pt-BR.json')

    def test_caption_wrap_preserves_localized_words(self):
        source=(ROOT/'src/menu/port_tbo.c').read_text(encoding='utf8')
        start=source.index('static s32 Tb_HelpWrap(');end=source.index('\nstatic void Tb_DrawHelpCaption',start)
        code=r'''
#include <assert.h>
#include <string.h>
typedef int s32;typedef float f32;
static float Ps_MsgWidthLiteral(const char *text,float scale){return strlen(text)*scale;}
'''+source[start:end]+r'''
int main(void){
 char text[]="A partida rápida encontra jogadores que buscam o mesmo tipo de batalha.";
 char original[256],lines[3][128],joined[384]="";strcpy(original,text);
 int count=Tb_HelpWrap(text,lines,1,32);assert(count==3);
 for(int i=0;i<count;i++){assert(strlen(lines[i])<=32);if(i)strcat(joined," ");strcat(joined,lines[i]);}
 assert(!strcmp(text,original));assert(!strcmp(joined,original));
 assert(Tb_HelpWrap(text,lines,1,8)==4);
 return 0;
}
'''
        self.compile_run(code)

    def test_catalog_contains_complete_descriptions(self):
        data=json.loads((ROOT/'port/translations/port/pt-BR.json').read_text(encoding='utf8'))
        self.assertEqual(data['labels_count'],len(data['labels']))
        self.assertFalse(any(k.startswith('tbo/wrap-fix/') for k in data['labels']))
        for row in data['labels'].values():
            self.assertNotIn('WWWW',row['text'])

    def test_full_health_tiers_do_not_show_the_previous_colour(self):
        source=(ROOT/'src/battle/hud_a_d.c').read_text(encoding='utf8')
        start=source.index('void HudGauge_UpdateHp(void)');end=source.index('\n}',start)+2
        code=r'''
#include <assert.h>
#include <string.h>
typedef int s32;
#define HUD_GAUGE_BAR 10000
typedef struct{int x,y;} Point;
typedef struct{Point pos;int show,sub;} HudSprite;
typedef struct{int side,hpShown[2];struct{int hp[2];}cur;HudSprite sprites[16];} Gauge;
static Gauge storage,*gHudGauge=&storage;
static void HudSprite_Show(HudSprite *s,int show){s->show=show;}
static void HudSprite_Move(HudSprite *s,int dx,int dy){s->pos.x+=dx;s->pos.y+=dy;}
static void HudSprite_SetTex(HudSprite *s,int tex,int sub){assert(tex==8);s->sub=sub;}
'''+source[start:end]+r'''
int main(void){
 const int colours[]={3,4,1,1,5,5};
 for(int tier=1;tier<=6;tier++){
  memset(&storage,0,sizeof(storage));storage.hpShown[0]=storage.cur.hp[0]=tier*10000;
  HudGauge_UpdateHp();assert(storage.sprites[8].sub==colours[tier-1]);
  assert(storage.sprites[3].sub==colours[tier-1]);assert(storage.sprites[7].pos.x==160);
 }
 memset(&storage,0,sizeof(storage));storage.hpShown[0]=storage.cur.hp[0]=42000;
 HudGauge_UpdateHp();assert(storage.sprites[8].sub==5 && storage.sprites[3].sub==1);
 assert(storage.sprites[7].pos.x==32);
 memset(&storage,0,sizeof(storage));HudGauge_UpdateHp();assert(!storage.sprites[3].show);
 return 0;
}
'''
        self.compile_run(code)
