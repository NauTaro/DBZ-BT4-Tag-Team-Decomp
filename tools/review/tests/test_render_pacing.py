from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[3]

class UnlimitedRenderingTests(unittest.TestCase):
    def test_deadlines_without_frame_ceiling_or_simulation_acceleration(self):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('A C compiler is required')
        code=r'''
#include <assert.h>
#include <stdint.h>
#include "render_pacing.h"
int main(void) {
    uint64_t start=UINT64_C(9000000000000),tick=33366700;
    assert(GsPresent_UnlimitedTime(start,start,tick)==0.0f);
    assert(GsPresent_UnlimitedTime(start+tick,start,tick)==1.0f);
    assert(GsPresent_UnlimitedTime(start+tick*2,start,tick)==1.0f);
    assert(GsPresent_UnlimitedTime(start-1,start,tick)==0.0f);
    assert(GsPresent_UnlimitedTime(start+tick/2,start,tick)>.499f);
    assert(GsPresent_UnlimitedTime(start+tick/2,start,tick)<.501f);
    // At 0.1 ms per render there is no artificial 60/120/240 ceiling.
    uint64_t cost=100000,now=start;int renders=0;
    while(!GsPresent_UnlimitedDone(now,start,tick,cost,0)) { ++renders;now+=cost; }
    assert(renders>300 && now+cost>=start+tick);
    // An arriving simulation frame preempts more presentation immediately.
    assert(GsPresent_UnlimitedDone(start+cost,start,tick,cost,1));
    // An expensive frame is not followed by a burst of queued extra frames.
    assert(GsPresent_UnlimitedDone(start,start,tick,tick,0));
    assert(GsPresent_UnlimitedDone(start,start,tick,tick*2,0));
    // A delayed game tick never leads to unlimited catch-up rendering.
    assert(GsPresent_UnlimitedDone(start+tick*4,start,tick,cost,0));
    // A 60 Hz menu and a 30 Hz fight use their own unchanged tick duration.
    assert(GsPresent_UnlimitedDone(start+16666700,start,16666700,cost,0));
    assert(!GsPresent_UnlimitedDone(start+16666700,start,tick,cost,0));
    return 0;
}'''
        with tempfile.TemporaryDirectory() as tmp:
            d=Path(tmp);source=d/'pacing.c';source.write_text(code)
            exe=d/('pacing.exe' if os.name=='nt' else 'pacing')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            subprocess.run([cc,'-std=c99','-I'+str(ROOT/'port/src/gs'),str(source),'-o',str(exe)],check=True,**flags)
            subprocess.run([str(exe)],check=True,**flags)
