"""Compile the shipped guide and catalog, including the Portuguese path."""
from pathlib import Path
import json,os,shutil,subprocess,tempfile,unittest
from test_potara import function

ROOT=Path(__file__).resolve().parents[3]
class RankedRulesTests(unittest.TestCase):
    def test_every_active_guide_page_has_both_translations(self):
        cc=shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('C compiler required')
        guide=function((ROOT/'src/menu/port_tbo.c').read_text(encoding='utf-8'),'Tb_RankedGuideText')
        code=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "match_supabase.h"
#include "online_language.h"
typedef int s32;
static int language;
int Port_GameProfileEnglish(void){return language==0;}
int Port_GameProfilePortuguese(void){return language==2;}
'''+"static const char *"+guide+r'''
int main(){Bt4RankedView v={0};const char *original,*translated;
for(int policy=0;policy<=1;policy++){
v.forfeitPenalty=policy?100:0;
for(int monthly=0;monthly<=1;monthly++){
  v.streakScoring=v.monthlyReset=monthly;
  for(int page=0;page<3;page++){
    original=Tb_RankedGuideText(&v,page);assert(strlen(original)>40);
    for(language=1;language<=2;language++){
      translated=PortOnline_Text(original);assert(translated&&strcmp(original,translated));
      assert(strlen(translated)<1024);
    }
  }
}
}
language=2;assert(!strcmp(PortOnline_Text("Collection: %d/7"),"Coleção: %d/7"));
language=1;assert(!strcmp(PortOnline_Text("Collection: %d/7"),"Colección: %d/7"));
v.streakScoring=v.monthlyReset=1;
v.forfeitPenalty=0;
assert(!strstr(Tb_RankedGuideText(&v,0),"37%"));
assert(strstr(Tb_RankedGuideText(&v,0),"16 points"));
assert(strstr(Tb_RankedGuideText(&v,0),"+13"));
assert(strstr(Tb_RankedGuideText(&v,1),"1st at 00:00"));
v.forfeitPenalty=100;assert(strstr(Tb_RankedGuideText(&v,0),"100 points"));
PortOnline_Raw(1);{char raw[]="Collection: %d/7";assert(PortOnline_Text(raw)==raw);}
PortOnline_Raw(0);
puts("PASS: one-screen guide, active and legacy rules, ES/EN/PT and profile labels");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);src=p/'guide.c';exe=p/('guide.exe' if os.name=='nt' else 'guide')
            src.write_text(code,encoding='utf-8')
            opts={'creationflags':0x08000000} if os.name=='nt' else {}
            result=subprocess.run((['rtk','proxy'] if shutil.which('rtk') else [])+[cc,'-w','-std=c99','-I'+str(ROOT/'port/src/gs'),str(src),str(ROOT/'port/src/gs/online_language.c'),'-o',str(exe)],capture_output=True,text=True,**opts)
            self.assertEqual(result.returncode,0,result.stderr)
            env={k:v for k,v in os.environ.items() if k not in ('BT4_RENDER_TEST','BT4_UI_LANGUAGE_TEST')}
            result=subprocess.run((['rtk','proxy'] if shutil.which('rtk') else [])+[str(exe)],env=env,capture_output=True,text=True,timeout=10,**opts)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
