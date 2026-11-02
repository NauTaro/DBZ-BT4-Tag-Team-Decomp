from pathlib import Path
import unittest,tempfile,subprocess,os,shutil,json,importlib.util,sys,re
ROOT=Path(__file__).resolve().parents[3]
class ScriptModsTests(unittest.TestCase):
    def test_tag_view_pair_and_camera_zoom_are_forwarded(self):
        cc=shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('C compiler required')
        source=(ROOT/'port/src/mods_game.c').read_text(encoding='utf8')
        functions=[]
        for name in ('PortMods_ViewActor','PortMods_CamSet'):
            match=re.search(r'(?:int|void) '+name+r'\([^\n]+\) \{.*?^\}',source,re.M|re.S)
            self.assertIsNotNone(match);functions.append(match[0])
        code='#include <assert.h>\n#include <string.h>\n#include "'+(ROOT/'port/src/gs/mods.h').as_posix()+'"\n'+'''
static int tag,own=7,target=9;static ModsCam sCam;static int sCamOut;
int TagBattle_Active(void){return tag;}int TagBattle_Count(void){return 10;}
int TagBattle_ViewActor(void){return own;}int TagBattle_Target(int actor){assert(actor==own || own<0 || own>=10);return target;}
'''+ '\n'.join(functions)+'''
int main(void){ModsCam in={0};assert(PortMods_ViewActor(0)==0 && PortMods_ViewActor(1)==1);
tag=1;assert(PortMods_ViewActor(0)==7 && PortMods_ViewActor(1)==9);target=-1;assert(PortMods_ViewActor(1)==7);
own=42;assert(PortMods_ViewActor(0)==0);in.zoom=.75f;in.collide=1;in.pos[0]=120;PortMods_CamSet(&in);
assert(sCamOut && sCam.zoom==.75f && sCam.collide==1 && sCam.pos[0]==120);in.collide=0;PortMods_CamSet(&in);assert(!sCam.collide);return 0;}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp);(path/'probe.c').write_text(code,encoding='utf8');exe=path/('probe.exe' if os.name=='nt' else 'probe')
            opts={'creationflags':0x08000000} if os.name=='nt' else {}
            p=subprocess.run([cc,str(path/'probe.c'),'-o',str(exe)],capture_output=True,text=True,**opts);self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True,**opts);self.assertEqual(p.returncode,0,p.stdout+p.stderr)
    def test_packaging_includes_mods_without_changing_user_scripts(self):
        sys.path.insert(0,str(ROOT/'port/tools'))
        try:
            spec=importlib.util.spec_from_file_location('mods_package',ROOT/'port/tools/package.py');package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)
            with tempfile.TemporaryDirectory() as tmp:
                out=Path(tmp);package.take_example_mods(out)
                for name in ('modern_camera','modern_hud','modern_look','skill_list','example_camera','example_hud'):
                    self.assertEqual((out/'mods'/name/'mod.lua').read_bytes(),(ROOT/'port/examples/mods'/name/'mod.lua').read_bytes())
                    self.assertEqual((out/'tbo/bundled-mods'/name/'mod.lua').read_bytes(),(out/'mods'/name/'mod.lua').read_bytes())
        finally:sys.path.pop(0)
        update=(ROOT/'port/src/gs/net_update.c').read_text(encoding='utf8')
        self.assertIn('if (user_file(rel) && !new_bundled_mod(rel))',update)
        self.assertIn('"bt3_mods.txt"',update)
    def test_new_labels_keep_all_format_placeholders(self):
        text=(ROOT/'port/src/gs/mods_locale.inc').read_text(encoding='utf8')
        rows=[json.loads('['+line.strip().rstrip(',')[1:-1]+']') for line in text.splitlines() if line.strip().startswith('{')]
        self.assertGreater(len(rows),60)
        for row in rows:
            expected=re.findall(r'%(?:\d+\$)?[sdif]',row[0])
            for translation in row[1:]:self.assertEqual(re.findall(r'%(?:\d+\$)?[sdif]',translation),expected,row)
