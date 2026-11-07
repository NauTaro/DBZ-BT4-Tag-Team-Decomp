"""Rush stage effects retain pointer types across the native/game ABI boundary."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from test_potara import function
ROOT=Path(__file__).resolve().parents[3]
class RushStagePointerTests(unittest.TestCase):
 def test_start_stop_preserves_high_pointer_bits_and_bounds(self):
  cc=shutil.which('gcc') or shutil.which('cc')
  if not cc:self.skipTest('C compiler required')
  source=(ROOT/'src/battle/eft_k.c').read_text(encoding='utf8')
  self.assertNotIn('#define EftStreak_Stop',source)
  header=(ROOT/'include/battle/eft_l.h').read_text(encoding='utf8')
  self.assertIn('void *stageFx[8]',header)
  code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef int s32;typedef float f32;
typedef struct {float unk60[8];int unk58[8],unk80;} Def;
typedef struct {int objId;Def *def;} EftTechArg;
typedef struct {EftTechArg *arg;int stage,flags;void *stageFx[8];} EftRushShotWork;
typedef struct {EftRushShotWork *work;} EftTask;
#define EFT_RUSHSHOT_STAGE_FX 0x200000
static int bits,starts,stops;
static uintptr_t expected;
static s32 BtlCharApi_GetOpponentObjId(s32 id){return 1;}
static int EftShot_TestBits(s32 id,s32 bit){return bits&bit;}
static f32 EftMath_WrapAngle(f32 a){return a;}
static void *EftStreak_Start(s32 obj,s32 kind,s32 color,f32 angle){++starts;return (void*)expected;}
static void EftStreak_Stop(void *ptr){assert((uintptr_t)ptr==expected);++stops;}
'''+function(source,'void EftRushShot_UpdateStageFx(')+r'''
int main(void){
 Def def={0};EftTechArg arg={3,&def};EftRushShotWork w={0};EftTask task={&w};w.arg=&arg;
 expected=sizeof(void*)>4?((uintptr_t)0x100<<32)|0x1a0ac40u:0x1a0ac40u;
 for(int i=0;i<8;++i){bits=0x80;EftRushShot_UpdateStageFx(&task);
  assert(w.stage==i+1&&(uintptr_t)w.stageFx[i]==expected);bits=0x100;EftRushShot_UpdateStageFx(&task);assert(!w.stageFx[i]);}
 assert(starts==8&&stops==8);bits=0x80;EftRushShot_UpdateStageFx(&task);assert(starts==8&&w.stage==8);
 w.stage=-1;EftRushShot_UpdateStageFx(&task);assert(starts==8);
 w.stage=100;w.flags=EFT_RUSHSHOT_STAGE_FX;w.stageFx[7]=(void*)expected;bits=0x100;EftRushShot_UpdateStageFx(&task);assert(stops==9);
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   d=Path(tmp);src=d/'probe.c';exe=d/('probe.exe' if os.name=='nt' else 'probe');src.write_text(code)
   flags={'creationflags':0x08000000} if os.name=='nt' else {}
   result=subprocess.run([cc,str(src),'-o',str(exe)],capture_output=True,text=True,**flags)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   subprocess.run([str(exe)],check=True,timeout=5,**flags)
