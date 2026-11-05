"""Explicit font choices survive language selection; Original remains available."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from test_potara import function
ROOT=Path(__file__).resolve().parents[3]

class NativePresentationTests(unittest.TestCase):
    def test_selected_fonts_take_precedence_only_on_matching_textures(self):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('A C compiler is required')
        source=(ROOT/'port/src/gs/menu_text.c').read_text(encoding='utf8')
        code=r'''
#include <assert.h>
static int sLock,sFontChoice,sBattleChoice,sSharedChoice;
static void pthread_mutex_lock(int *p){assert(p==&sLock);}
static void pthread_mutex_unlock(int *p){assert(p==&sLock);}
static void *shared_sheet(int tbp,int cbp){return tbp==100&&cbp>=90&&cbp<94?(void*)1:0;}
static int label_size(int width,int height){return width==256&&height==128;}
'''+function(source,'int MenuText_IsSharedTexture(int tbp, int cbp) {')+function(source,'int MenuText_WantsTexture(int width, int height, int indexed, int tbp, int cbp) {')+r'''
int main(void){
 assert(!MenuText_WantsTexture(256,128,1,100,90));
 assert(MenuText_IsSharedTexture(100,93)&&!MenuText_IsSharedTexture(100,94));
 sSharedChoice=1;
 assert(MenuText_WantsTexture(256,128,1,100,90));
 assert(!MenuText_WantsTexture(256,128,1,200,90));
 sSharedChoice=0;sFontChoice=2;
 assert(MenuText_WantsTexture(256,128,1,200,90));
 assert(!MenuText_WantsTexture(256,128,0,200,90));
 assert(!MenuText_WantsTexture(512,512,1,200,90));
 sFontChoice=0;sBattleChoice=3;
 assert(MenuText_WantsTexture(256,128,1,200,90));
 sBattleChoice=0;
 assert(!MenuText_WantsTexture(256,128,1,200,90));
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'probe.c').write_text(code);exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99',str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_language_save_does_not_change_explicit_original_or_custom_font(self):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('A C compiler is required')
        source=(ROOT/'port/src/gs/menu_translation.c').read_text(encoding='utf8')
        code=r'''
#include <assert.h>
#include <string.h>
static int sBootChoiceMade,writes;static char saved[16];
static void Port_SettingTextSave(const char *key,const char *value){assert(!strcmp(key,"menu_lang"));strcpy(saved,value);}
static void Port_SettingsWrite(void){++writes;}
'''+function(source,'void MenuTranslation_SaveChoice(const char *locale) {')+r'''
int main(void){
 const char *langs[]={"en","es","pt-BR"};
 for(int i=0;i<3;++i){MenuTranslation_SaveChoice(langs[i]);assert(!strcmp(saved,langs[i])&&sBootChoiceMade);}
 assert(writes==3);MenuTranslation_SaveChoice(0);MenuTranslation_SaveChoice("");assert(writes==3);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'probe.c').write_text(code);exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99',str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_boot_language_preserves_user_font_choices(self):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('A C compiler is required')
        source=(ROOT/'port/src/gs/menu_text.c').read_text(encoding='utf8')
        code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int sLock,sBootAwaitLanguage,sFontCount=1,sLabelsThere,sLabelsOld;
static char sLabelDir[4]="txt";
static struct {int uses[4];}sFonts[1];
#define FONT_MENU 1
#define FONT_BATTLE 3
static int menuFont=99,battleFont=98,sharedFont=97,loaded;
static int saved[3];
static int Port_Setting(const char *name,int fallback){
 if(!strcmp(name,"menu_font_id"))return saved[0];
 if(!strcmp(name,"battle_font_id"))return saved[1];
 if(!strcmp(name,"text_font_id"))return saved[2];
 return fallback;
}
static void pthread_mutex_lock(int *p){assert(p==&sLock);}
static void pthread_mutex_unlock(int *p){assert(p==&sLock);}
static void menu_text_dirs(void){++loaded;}
static void labels_start(void){}
static void MenuText_SetFontId(int id){menuFont=id;}
static void MenuText_SetBattleFontId(int id){battleFont=id;}
static void MenuText_SetSharedFontId(int id){sharedFont=id;}
'''+function(source,'void MenuText_BootLanguageChosen(void) {')+r'''
int main(void){
 for(int language=0;language<3;++language){
  for(int custom=0;custom<2;++custom){
   saved[0]=custom?123:0;saved[1]=custom?456:0;saved[2]=custom?789:0;
   sBootAwaitLanguage=1;menuFont=99;battleFont=98;sharedFont=97;
   MenuText_BootLanguageChosen();
   assert(!sBootAwaitLanguage && menuFont==saved[0] && battleFont==saved[1] && sharedFont==saved[2]);
  }
 }
 assert(loaded==6);MenuText_BootLanguageChosen();assert(loaded==6);
 puts("Font settings: deferred activation preserves explicit choices and Original in all languages PASS");
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);(d/'probe.c').write_text(code);exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99',str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
if __name__=='__main__':unittest.main()
