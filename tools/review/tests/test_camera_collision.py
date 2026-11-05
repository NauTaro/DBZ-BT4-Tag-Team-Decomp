"""Camera rays reject unrelated stage records without changing collision state."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
from test_potara import function
ROOT=Path(__file__).resolve().parents[3]
class CameraCollisionTests(unittest.TestCase):
 def test_real_triangle_and_unbound_offsets_cycle_and_bad_indices(self):
  cc=shutil.which('gcc') or shutil.which('cc')
  if not cc:self.skipTest('C compiler required')
  source=(ROOT/'src/battle/spectator_collision.inc').read_text(encoding='utf8')
  code=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <math.h>
typedef float f32;typedef int32_t s32;typedef uintptr_t u32;
typedef struct {float min[3],max[3];}ColBox;
typedef struct {ColBox box;s32 left,right;}ColNode;
typedef struct {s32 flags,vtx[3];float nrm[4];}ColPoly;
typedef struct {float x,y,z,w;}ColVec;
typedef struct {s32 nodeCount,unk4,unk8,nodeOfs,polyOfs,vtxOfs;}ColMesh;
static s32 *gColMeshBase;static u32 sSpecBlobBase,sSpecBlobBytes;
'''
  code+='#include "'+(ROOT/'port/src/gs/spectator_camera.h').as_posix()+'"\n'
  for sig in ('static s32 SpecCamera_Box(', 'static s32 SpecCamera_Range(', 'static void SpecCamera_Mesh('):code+=function(source,sig)+'\n'
  code+=r'''
int main(void){
 union {double align;unsigned char data[2048];} arena;memset(&arena,0,sizeof(arena));
 gColMeshBase=(s32*)arena.data;sSpecBlobBase=(u32)arena.data;sSpecBlobBytes=sizeof(arena.data);
 ColMesh *mesh=(ColMesh*)arena.data;mesh->nodeCount=1;mesh->nodeOfs=16;mesh->polyOfs=32;mesh->vtxOfs=48;
 ColNode *node=(ColNode*)(arena.data+64);node->box=(ColBox){{-1,-1,-.01f},{1,1,.01f}};node->left=0;node->right=-1;
 ColPoly *poly=(ColPoly*)(arena.data+128);poly->vtx[0]=0;poly->vtx[1]=1;poly->vtx[2]=2;
 ColVec *v=(ColVec*)(arena.data+192);v[0]=(ColVec){-1,-1,0,1};v[1]=(ColVec){1,-1,0,1};v[2]=(ColVec){0,1,0,1};
 float a[3]={0,0,-1},b[3]={0,0,1},hit=1;SpecCamera_Mesh(mesh,a,b,&hit);assert(fabsf(hit-.5f)<.0001f);
 mesh->nodeOfs=-1;hit=1;SpecCamera_Mesh(mesh,a,b,&hit);assert(hit==1);
 mesh->nodeOfs=16;mesh->nodeCount=0x7fffffff;SpecCamera_Mesh(mesh,a,b,&hit);assert(hit==1);
 mesh->nodeCount=1;poly->vtx[2]=0x7fffffff;SpecCamera_Mesh(mesh,a,b,&hit);assert(hit==1);
 poly->vtx[2]=2;node->left=0x7fffffff;SpecCamera_Mesh(mesh,a,b,&hit);assert(hit==1);
 node->left=0;node->right=0;SpecCamera_Mesh(mesh,a,b,&hit);assert(hit==1);
 assert(!SpecCamera_Range((void*)(sSpecBlobBase+2040),24));assert(!SpecCamera_Range((void*)(sSpecBlobBase-1),1));
}
'''
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);src=p/'probe.c';exe=p/('probe.exe' if os.name=='nt' else 'probe');src.write_text(code)
   flags={'creationflags':0x08000000} if os.name=='nt' else {}
   r=subprocess.run([cc,'-std=c99',str(src),'-lm','-o',str(exe)],capture_output=True,text=True,**flags)
   self.assertEqual(r.returncode,0,r.stdout+r.stderr);subprocess.run([str(exe)],check=True,timeout=5,**flags)
