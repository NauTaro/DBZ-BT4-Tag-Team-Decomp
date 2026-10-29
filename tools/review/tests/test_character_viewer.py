"""Native viewer pose selection and resource ownership without disc data."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest

ROOT = Path(__file__).resolve().parents[3]


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        if source[end] == '{':
            depth += 1
        elif source[end] == '}':
            depth -= 1
        end += 1
    return source[start:end]


class CharacterViewerTests(unittest.TestCase):
    def test_pose_precedes_playback_and_survives_model_replacement(self):
        cc = shutil.which('cc') or shutil.which('gcc')
        if not cc:
            self.skipTest('A C compiler is required')
        source = (ROOT / 'src/battle/view_a_d.c').read_text(encoding='utf8')
        code = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef int s32;typedef unsigned char u8;typedef unsigned u32;typedef float f32;
#define PORT 1
#define CHRVIEW_SLOT_COUNT 2
#define PORT_V(a,b) (gPortVariant?(b):(a))
#define BTL_OBJ_FLAGS(obj) (*(u32 *)((obj)+0xa40))
typedef struct {int node,obj,file;} ChrViewSlot;
typedef struct {int state,count,chara,costume,damaged;ChrViewSlot *slot;u8 *obj;} ChrViewJob;
typedef struct {char pad[0x30];float targetY,dist,minDist;} ChrViewInfo;
typedef struct {int delay,anim,zoom,stageOn,visible,loading,free;ChrViewSlot *cur;ChrViewSlot slot[2];} ChrView;
static ChrView work,*gChrView=&work;static int gPortVariant=1;
static void *sBt4ViewerMotion[2];
static union {void *align;u8 bytes[0x1000];} objects[4];
static char banks[3][32];static int destroyed[4],bankFreed[3],bankObject[3];
static int created,loaded,freed,played;static float minimum;
static ChrViewInfo info={{0},-8.0f,20.0f,6.5f};
static ChrViewInfo *ChrView_GetCharaInfo(s32 c){return &info;}
static void OrbitCam_Reset(void){}
static void OrbitCam_SetFloorClamp(int n){}
static void OrbitCam_SetTarget(float a,float b,float c){}
static void OrbitCam_SetAngles(float a,float b,float c){}
static void OrbitCam_SetDist(float n){}
static void OrbitCam_SetDistRange(float a,float b){minimum=a;}
static void OrbitCam_SetPitchLimit(float n){}
static void Heap_Free(void *p){
 int i;for(i=0;i<3;i++)if(p==banks[i]){
  assert(!bankFreed[i]&&destroyed[bankObject[i]]);bankFreed[i]=1;++freed;return;
 }assert(0);
}
static void SList_PushFront(void *a,void *b){}
static void ChrView_SetLoading(s32 n){gChrView->loading=n;}
static ChrViewSlot *ChrView_AllocSlot(void){return &gChrView->slot[0];}
static int ChrView_GetModelFile(int a,int b,int c){return 1;}
static void BtlRes_LoadSingle(int a,void *b,int c){}
static int File_UpdateRequests(void){return 1;}
static int BtlObj_Create(int a,void *b,int c){return created++;}
static u8 *BtlObj_Get(int id){return objects[id].bytes;}
static void *File_LoadSync(int id,void *p,int n){
 assert(id==0xf3c||id==0xf3c+78);assert(p==0&&n==0&&loaded<3);
 bankObject[loaded]=created-1;return banks[loaded++];
}
static void BtlObjAnim_PlayModel(u8 *p,int animation,int mode){
 assert(animation==7&&mode==2&&loaded>0);
 assert(*(void**)(p+0x754)==banks[loaded-1]);++played;
}
static void BtlObj_SetPartVisible(u8 *obj,unsigned part,int on){}
static void BtlObj_Destroy(int id){assert(!destroyed[id]);destroyed[id]=1;}
static ChrViewSlot *ChrView_GetCur(void){return gChrView->cur;}
static void ChrView_SetCur(ChrViewSlot *s){gChrView->cur=s;}
static void StgAmb_Init(void){}
'''
        for signature in ['s32 ChrView_FreeSlot(', 'void ChrView_SetupCamera(', 's32 ChrView_StepLoad(']:
            code += '\n' + function(source, signature) + '\n'
        code += r'''
static void finish(ChrViewJob *job){int i;for(i=0;i<4&&job->state==3;++i)if(ChrView_StepLoad(job))return;assert(0);}
int main(void){
 ChrViewJob first={0},second={0},cancelled={0};
 gChrView->anim=7;gChrView->zoom=1;
 first.state=2;first.slot=&gChrView->slot[0];
 assert(!ChrView_StepLoad(&first)&&loaded==1&&played==1);finish(&first);
 assert(gChrView->cur==first.slot&&minimum==6.5f&&freed==0);
 second.state=2;second.chara=78;second.slot=&gChrView->slot[1];
 assert(!ChrView_StepLoad(&second)&&loaded==2&&played==2);
 assert(!bankFreed[0]&&sBt4ViewerMotion[0]!=sBt4ViewerMotion[1]);
 finish(&second);assert(gChrView->cur==second.slot&&freed==1&&!bankFreed[1]);
 cancelled.state=2;cancelled.slot=&gChrView->slot[0];
 assert(!ChrView_StepLoad(&cancelled)&&loaded==3);
 BtlObj_Destroy(cancelled.slot->obj);ChrView_FreeSlot(cancelled.slot);
 assert(freed==2&&!bankFreed[1]&&sBt4ViewerMotion[0]==0);
 BtlObj_Destroy(second.slot->obj);ChrView_FreeSlot(second.slot);
 assert(freed==3&&sBt4ViewerMotion[1]==0);
 gPortVariant=0;ChrView_SetupCamera(0);assert(minimum==3.0f);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            src, exe = root / 'viewer.c', root / ('viewer.exe' if os.name == 'nt' else 'viewer')
            src.write_text(code, encoding='utf8')
            flags = {'creationflags': 0x08000000} if os.name == 'nt' else {}
            result = subprocess.run([cc, '-std=c99', str(src), '-o', str(exe)], capture_output=True, text=True, timeout=30, **flags)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15, **flags)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
