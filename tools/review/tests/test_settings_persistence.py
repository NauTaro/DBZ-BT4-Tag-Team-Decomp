"""Large settings files and duplicate legacy entries remain writable across restarts."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

class SettingsPersistenceTests(unittest.TestCase):
    def test_music_beyond_old_capacity_and_duplicate_keys(self):
        cc=shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('C compiler required')
        code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
int Port_Setting(const char *,int);
void Port_SettingSave(const char *,int),Port_SettingsWrite(void);
int main(int argc,char **argv){
 assert(argc==2);
 if(!strcmp(argv[1],"save")){
  assert(Port_Setting("main_menu_bgm",0)==0x830);
  /* The album's selection writes this key; existing duplicate entries must
   * not make the next read return the previous music. */
  Port_SettingSave("main_menu_bgm",0x843);Port_SettingsWrite();
 }
 assert(Port_Setting("main_menu_bgm",0)==0x843);
 for(int i=0;i<400;++i){char key[24];snprintf(key,sizeof(key),"fixture_rule_%d",i);assert(Port_Setting(key,-1)==i);}
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);src=p/'probe.c';exe=p/('probe.exe' if os.name=='nt' else 'probe');src.write_text(code)
            settings=p/'settings.txt'
            settings.write_text('main_menu_bgm=2070\n'+''.join(f'fixture_rule_{i}={i}\n' for i in range(400))+'main_menu_bgm=2096\n')
            env=dict(os.environ,BT3_SETTINGS=str(settings));hidden={'creationflags':0x08000000} if os.name=='nt' else {}
            commands=[[cc,'-std=gnu99',str(src),str(ROOT/'port/src/plat_settings.c'),'-lpthread','-o',str(exe)],[str(exe),'save'],[str(exe),'read']]
            prefix=['rtk','proxy'] if shutil.which('rtk') else []
            for args in commands:
                proc=subprocess.run(prefix+args,env=env,capture_output=True,text=True,timeout=20,**hidden)
                self.assertEqual(proc.returncode,0,proc.stdout+proc.stderr)
            text=settings.read_text();self.assertEqual(text.count('main_menu_bgm='),1)

if __name__=='__main__':unittest.main()
