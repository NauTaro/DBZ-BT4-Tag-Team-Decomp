"""The native untextured mask clear must follow its textured HUD panel."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest

ROOT = Path(__file__).resolve().parents[3]

class HudMaskTests(unittest.TestCase):
    def test_extended_layout_survives_saved_setting_loading(self):
        source = (ROOT / 'port/src/gs/gs_draw.c').read_text(encoding='utf8')
        start = source.index('    sHudLayout = getenv(')
        line = source[source.index('\n', start) + 1:].splitlines()[0].strip()
        expression = line.split('=', 1)[1].strip().rstrip(';')
        for value, expected in [(-1, 0), (0, 0), (1, 1), (2, 2), (3, 0)]:
            ternary = expression.replace('sHudLayout', str(value))
            cond, choices = ternary.split('?')
            yes, no = choices.split(':')
            cond = cond.replace('||', ' or ')
            actual = int(yes if eval(cond) else no)
            self.assertEqual(actual, expected)

    def test_mask_and_colour_take_the_same_widescreen_route(self):
        cc = shutil.which('cc') or shutil.which('gcc')
        if not cc: self.skipTest('A C compiler is required')
        source = (ROOT / 'port/src/gs/gs_draw.c').read_text(encoding='utf8')
        start = source.index('if (gsAnchor != 20 && Port_IsWide()')
        end = source.index(' {', start)
        condition = source[start + 4:end - 1]
        code = '''#include <assert.h>
#include <stdint.h>
static int gsAnchor,gPortMenuMode,sWholeFrame,tbo,hudPanel,type,fst,battleText,wide=1,gGsMainFbp;
static struct {int tex_is_target,target;} d;
static struct {uint32_t fbp;} gsTargets[1];
static int Port_IsWide(void){return wide;}
static int Port_MenusWide(void){return 1;}
static int Port_PageAspectMilli(void){return 2333;}
static int applies(void){return (''' + condition + ''');}
int main(void){
 int a;
 for(a=1;a<=6;++a){
   gsAnchor=a;hudPanel=1;type=3;fst=0;assert(applies());
   fst=1;assert(applies());
   d.tex_is_target=1;fst=0;assert(applies());d.tex_is_target=0;
 }
 hudPanel=0;gsAnchor=0;type=3;fst=0;assert(!applies());
 hudPanel=1;gsAnchor=5;wide=0;assert(!applies());
 wide=1;sWholeFrame=1;assert(!applies());
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder); c=path/'mask.c'; c.write_text(code)
            exe=path/('mask.exe' if os.name=='nt' else 'mask')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            prefix=['rtk','proxy'] if shutil.which('rtk') else []
            p=subprocess.run(prefix+[cc,'-std=c99',str(c),'-o',str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run(prefix+[str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
