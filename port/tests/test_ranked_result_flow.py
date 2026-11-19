"""Compile production result ownership and requeue decisions without opening a window."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
def function(text,signature):
    a=text.index(signature);b=text.index('{',a)+1;depth=1
    while depth:depth+=(text[b]=='{')-(text[b]=='}');b+=1
    return text[a:b]
class RankedResultFlowTests(unittest.TestCase):
 def test_winner_owns_menu_and_search_exits_instead_of_rematching(self):
    cc=shutil.which('gcc') or shutil.which('cc')
    self.assertIsNotNone(cc,'C compiler required')
    seq=(ROOT/'src/battle/btl_seq.c').read_text(encoding='utf8');hud=(ROOT/'src/battle/hud_0.c').read_text(encoding='utf8')
    code='''
#include <assert.h>
#include <stdio.h>
typedef int s32;
#define BTL_RESULT_ABORT 8
static int online=1,ranked=1,winner=1,hasWinner=1,padCount=1,owner=-1,local=0,requeue=0,reason=0,flags=0;
int Port_NetActive(void){return online;} int Port_RankedActive(void){return ranked;}
int BattleResult_HasWinner(void){return hasWinner;} int BattleResult_GetWinnerSide(void){return winner;}
int BtlPause_GetPadCount(void){return padCount;} void BtlPause_SetPad(int p){owner=p;}
int Port_NetMe(void){return local;} void Port_NetRequestRankedRequeue(void){requeue++;}
void BattleResult_Set(int f,int r){flags=f;reason=r;}
'''+function(seq,'void BtlSeq_SetResultPad(void) {')+'\n'+function(hud,'static s32 PauseMenu_RankedRequeue(')+'''
int main(void){
 for(winner=0;winner<2;winner++)for(local=0;local<2;local++){
   owner=-1;BtlSeq_SetResultPad();assert(owner==winner);
   requeue=0;reason=flags=0;
   assert(PauseMenu_RankedRequeue(owner,1,0x13)==1);
   assert(requeue==(local==winner));
   assert(flags==BTL_RESULT_ABORT&&reason==8&&!(reason&0x18000));
 }
 winner=1;ranked=0;BtlSeq_SetResultPad();assert(owner==0);
 assert(!PauseMenu_RankedRequeue(1,1,0x13));
 ranked=1;online=0;BtlSeq_SetResultPad();assert(owner==0);
 online=1;hasWinner=0;BtlSeq_SetResultPad();assert(owner==0);
 assert(!PauseMenu_RankedRequeue(1,0,0x13));assert(!PauseMenu_RankedRequeue(1,1,0xF));
 puts("PASS: either winning seat controls Ranked result; requeue leaves session, never rematches");
}
'''
    with tempfile.TemporaryDirectory() as tmp:
     p=Path(tmp);src=p/'result.c';exe=p/'result.exe';src.write_text(code,encoding='utf8')
     opts={'creationflags':0x08004000} if os.name=='nt' else {}
     for args in [[cc,'-std=c99','-DPORT',str(src),'-o',str(exe)],[str(exe)]]:
      r=subprocess.run(['rtk','proxy',*args],capture_output=True,text=True,timeout=25,**opts)
      self.assertEqual(r.returncode,0,r.stdout+r.stderr)
