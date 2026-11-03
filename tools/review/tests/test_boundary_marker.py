"""Reused effect textures must not identify unrelated triangles as arena walls."""
from pathlib import Path
import os, re, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[3]

class BoundaryMarkerTests(unittest.TestCase):
    def test_packet_identity_survives_both_primitive_modes(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        core=(ROOT/'port/src/gs/gs_core.c').read_text(encoding='utf8')
        draw=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
        start=core.index('static void prim_update(void) {')
        update=core[start:core.index('\n}',start)+2]
        start=draw.index('if (type == 3 && (prim & PORT_PRIM_BOUNDARY)')
        end=draw.index(' {',start)
        condition=draw[start+4:end-1]
        packed=re.search(r'case 0x0: reg_write\(0, (.*?)\); break;',core)[1]
        code='''#include <stdint.h>
#include <assert.h>
#include "port_render_markers.h"
static struct {uint64_t prim,primRaw,prmode,prmodecont;} gs;
static int type=3,tag;
static uint64_t prim;
static int Port_TagBattleRenderActive(void){return tag;}
'''+update+'\nstatic int boundary(void){return ('+condition+''');}
int main(void){
 uint64_t lo=PORT_PRIM_BOUNDARY|0x1B;
 gs.primRaw='''+packed+''';gs.prmodecont=1;prim_update();
 assert(gs.prim==gs.primRaw);prim=gs.prim;assert(boundary());
 gs.prmodecont=0;gs.prmode=0x740;prim_update();
 assert((gs.prim&0x7FF)==0x743);assert(gs.prim&PORT_PRIM_BOUNDARY);
 prim=0x1B;assert(!boundary());
 prim=PORT_PRIM_BOUNDARY|0x1B;type=6;assert(!boundary());
 type=3;tag=1;assert(!boundary());
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);src=path/'marker.c';src.write_text(code)
            exe=path/('marker.exe' if os.name=='nt' else 'marker')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            prefix=['rtk','proxy'] if shutil.which('rtk') else []
            p=subprocess.run(prefix+[cc,'-std=c99','-I'+str(ROOT/'port/include'),str(src),'-o',str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run(prefix+[str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
