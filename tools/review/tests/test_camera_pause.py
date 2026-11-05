"""Pause holds presentation state without applying a previous actor's camera."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from test_potara import function
ROOT=Path(__file__).resolve().parents[3]
class CameraPauseTests(unittest.TestCase):
 def test_pause_resume_actor_switch_cut_disable_and_resimulation(self):
  cc=shutil.which('gcc') or shutil.which('cc')
  if not cc:self.skipTest('C compiler required')
  source=(ROOT/'port/src/mods_game.c').read_text(encoding='utf8')
  code='#include <assert.h>\n#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n#include "'+(ROOT/'port/src/gs/mods.h').as_posix()+'"\n'+r'''
#define GAME_PTR *
#define CHR_CAM_TARGET 0
static int gPortResim,gsSkipFrame,active=1,cut,calls,sCamOut;
static ModsCam sCam;static int sZoom[2]={1000,1000},sCollide[2];
static struct {int valid,obj;ModsCam pose;}sPausedCamera[2];
static float written[2][3];static unsigned char chr[32];
static void *BtlChar_FindByObjId(int id){return chr;}
static int ChrCam_IsCutActive(void *p){return cut;}
static int BtlChar_TestFlag(void *p,unsigned n){return 0;}
static int PortSpectator_Mode(void){return 0;}
int Mods_WantCamera(void){return active;}
static void rdf(float *out,unsigned char *p,int offset,int count){memset(out,0,count*4);}
static void rda(float *out,uint32_t addr,int count){memset(out,0,count*4);}
static void wra(uint32_t addr,const float *in,int count){memcpy(written[addr],in,count*4);}
void Mods_Camera(void){++calls;if(sCam.cut)return;sCam.pos[0]=(float)calls;sCam.zoom=.8f;sCam.collide=1;sCamOut=1;}
'''+function(source,'void Port_ModsCamera(int side, int objId, int pos, int rot, int paused) {')+r'''
int main(void){
 Port_ModsCamera(0,3,0,1,0);assert(calls==1&&written[0][0]==1&&sZoom[0]==800);
 for(int i=0;i<100;++i)Port_ModsCamera(0,3,0,1,1);
 assert(calls==1&&written[0][0]==1);
 Port_ModsCamera(1,8,0,1,0);assert(calls==2);
 Port_ModsCamera(0,3,0,1,1);assert(calls==2&&written[0][0]==1);
 Port_ModsCamera(0,4,0,1,1);assert(calls==3&&written[0][0]==3);
 cut=1;Port_ModsCamera(0,4,0,1,1);assert(calls==4&&!sPausedCamera[0].valid);
 cut=0;Port_ModsCamera(0,4,0,1,1);assert(calls==5&&written[0][0]==5);
 gPortResim=1;Port_ModsCamera(0,4,0,1,0);assert(calls==5&&sZoom[0]==800);gPortResim=0;
 /* A renderer skip must still write the mod pose into this tick's list. */
 gsSkipFrame=1;Port_ModsCamera(0,4,0,1,0);assert(calls==6&&written[0][0]==6&&sZoom[0]==800);gsSkipFrame=0;
 active=0;Port_ModsCamera(0,4,0,1,1);assert(!sPausedCamera[0].valid&&sZoom[0]==1000);
 active=1;Port_ModsCamera(0,4,0,1,1);assert(calls==7);
 Port_ModsCamera(0,4,0,1,0);assert(calls==8&&written[0][0]==8);
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   d=Path(tmp);src=d/'probe.c';exe=d/('probe.exe' if os.name=='nt' else 'probe');src.write_text(code)
   kwargs={'creationflags':0x08000000} if os.name=='nt' else {}
   built=subprocess.run([cc,str(src),'-o',str(exe)],capture_output=True,text=True,**kwargs)
   self.assertEqual(built.returncode,0,built.stdout+built.stderr)
   subprocess.run([str(exe)],check=True,**kwargs)
