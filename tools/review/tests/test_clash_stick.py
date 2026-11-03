"""Exercise the local stick adapter before its pad bytes enter networking."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest

ROOT = Path(__file__).resolve().parents[3]


class ClashStickTests(unittest.TestCase):
    def test_sectors_release_buttons_and_online_viewer(self):
        cc = shutil.which('cc') or shutil.which('gcc')
        if not cc:
            self.skipTest('A C compiler is required')
        source = (ROOT / 'port/src/gs/gs_input.c').read_text(encoding='utf8')
        start = source.index('static void clash_stick(')
        end = source.index('\n}\n', start) + 3
        code = '''#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#define B_SQUARE 1
#define B_TRIANGLE 2
static int enabled=1,net,me,action=0x130,lastActor;
int Port_ClashStick(void){return enabled;}
int Port_NetActive(void){return net;}
int Port_NetMe(void){return me;}
int PortMods_ViewActor(int side){return side+4;}
void *BtlChar_FindBySide(int side){lastActor=side;return &action;}
int BtlAct_GetCurrent(void *chr){return *(int*)chr;}
''' + source[start:end] + '''
static unsigned sample(int socket,int x,int y){
 unsigned btn=8;clash_stick(socket,&btn,x,y);return btn;
}
int main(void){
 assert(sample(0,128,128)==8);
 assert(sample(0,255,128)==11);assert(lastActor==4);
 assert(sample(0,128,255)==8); /* explicit release between presses */
 assert(sample(0,128,255)==11);
 assert(sample(0,128,255)==8);assert(sample(0,128,255)==8);
 assert(sample(0,128,128)==8);assert(sample(0,128,255)==11);
 action=0xB;assert(sample(0,255,128)==8);
 action=0xFA;assert(sample(0,255,128)==11);
 enabled=0;assert(sample(0,128,255)==8);
 enabled=1;net=1;me=1;assert(sample(0,128,255)==11);assert(lastActor==5);
 assert(sample(1,0,128)==11); /* each local socket has independent release state */
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            src = path / 'stick.c'
            src.write_text(code)
            exe = path / ('stick.exe' if os.name == 'nt' else 'stick')
            prefix = ['rtk', 'proxy'] if shutil.which('rtk') else []
            flags = {'creationflags': 0x08000000} if os.name == 'nt' else {}
            result = subprocess.run(prefix + [cc, '-std=c99', str(src), '-o', str(exe)],
                                    capture_output=True, text=True, **flags)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run(prefix + [str(exe)], capture_output=True, text=True, **flags)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
