"""Exercise the effect entry points identified in the reported native crashes."""
from pathlib import Path
import os, re, shutil, subprocess, tempfile, unittest

ROOT=Path(__file__).resolve().parents[3]

def function(path, name):
    source=(ROOT/path).read_text(encoding='utf-8')
    match=re.search(r'^\w[^\n]*\b'+re.escape(name)+r'\([^;\n]*\)\s*\{',source,re.M)
    if not match:raise ValueError(name)
    brace=source.index('{',match.start());end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[match.start():end]

class EffectLifecycleTests(unittest.TestCase):
    def compile_run(self, text):
        cc=shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('C compiler required')
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp);src=directory/'probe.c';exe=directory/('probe.exe' if os.name=='nt' else 'probe')
            src.write_text(text,encoding='utf-8')
            flags={'creationflags':0x08000000} if os.name=='nt' else {}
            p=subprocess.run([cc,'-std=gnu99','-DPORT','-I'+str(ROOT/'include'),str(src),'-o',str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)

    def test_ten_body_effects_have_independent_slots_and_safe_stop(self):
        code=r'''
#include <assert.h>
#include <string.h>
#include "battle/eft_o.h"
static struct {EftBodyFxMgr mgr;unsigned canary;} storage;
EftBodyFxMgr *gEftBodyFx=&storage.mgr;void *gEftBodyFxList=(void *)1;
static EftOTask tasks[10];static EftBodyFx work[10];
void *gEftBodyFxClass[6];
EftOTask *BtlTaskList_AddTail(void *list,void *cls,void *arg){
 EftBodyFxArg *a=arg;tasks[a->objId].work=&work[a->objId];
 work[a->objId].arg=*a;work[a->objId].flags=1;return &tasks[a->objId];
}
'''+function('src/battle/eft_o_c.c','EftBodyFx_Start')+'\n'+function('src/battle/eft_o_c.c','EftBodyFx_Stop')+r'''
int main(void){
 storage.canary=0xdeadbeef;
 for(int i=0;i<10;++i){EftBodyFxArg a={i,0,1};assert(EftBodyFx_Start(a));assert(!EftBodyFx_Start(a));}
 for(int i=0;i<10;++i){assert(storage.mgr.task[i]==&tasks[i]);assert(EftBodyFx_Stop(i));assert(work[i].flags==3);}
 assert(storage.canary==0xdeadbeef);assert(!EftBodyFx_Stop(-1)&&!EftBodyFx_Stop(10));
 tasks[9].work=0;assert(!EftBodyFx_Stop(9));tasks[9].work=&work[9];
 work[9].arg.objId=8;assert(!EftBodyFx_Stop(9));
 gEftBodyFx=0;assert(!EftBodyFx_Stop(0));EftBodyFxArg a={0,0,1};assert(!EftBodyFx_Start(a));
}
'''
        self.compile_run(code)

    def test_missing_or_reloading_shot_group_cannot_start_a_partial_timeline(self):
        code=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "battle/eft_h.h"
#define TYPE_ROW(t) ((t)+1)
EftHMgr manager,*gEftShot=&manager;
EftShotClassRow gEftShotClass[11];
static EftHChar actors[10];static int events,instances;
s32 BtlScene_GetCharCount(void){return 10;}
void EftShot_SetCurSlot(s32 a,s32 slot){}
void EftTechEvt_Start(EftHSlot *s,s32 a,s32 b){++events;}
s32 EftShot_GetLeadTime(EftHSlot *s){return 1;}
EftHTask *BtlTaskList_AddTail(void *list,void *cls,void *arg){assert(list&&cls);++instances;return (void *)1;}
'''+function('src/battle/eft_h.c','EftShot_GetSlot')+'\n'+function('src/battle/eft_h.c','EftShot_Start')+r'''
int main(void){
 manager.chars=actors;EftHStartArg arg={9,2,0,0,2,1,1};
 EftHSlot *s=&actors[9].slot[2];s->pack=(void *)1;s->param=&actors[9].param[2];
 s->param->type=0;s->param->life=99;EftHTask group={0};EftHTaskClass cls={0};
 gEftShotClass[1].inst=&cls;gEftShotClass[1].group=&cls;
 EftShot_Start(&arg);assert(!events&&!instances&&s->param->life==99);
 s->task=&group;group.list=actors[9].list=(void *)2;group.unk28[0]=&cls;
 EftShot_Start(&arg);assert(!events&&!instances&&s->param->life==99);
 group.children=(void *)3;group.kill=1;EftShot_Start(&arg);assert(!events&&!instances);
 group.kill=0;EftShot_Start(&arg);assert(events==1&&instances==1);
 assert(!EftShot_GetSlot(-1,0)&&!EftShot_GetSlot(10,0)&&!EftShot_GetSlot(0,6));
 EftShot_Start(0);arg.chr=10;EftShot_Start(&arg);assert(events==1&&instances==1);
}
'''
        self.compile_run(code)

if __name__=='__main__':unittest.main()
