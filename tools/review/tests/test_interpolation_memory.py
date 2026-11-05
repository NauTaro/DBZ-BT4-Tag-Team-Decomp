"""Saved presentation choices and allocation-failure recovery, using production blocks."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[3]

def block(source,marker):
    start=source.index(marker);at=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[at]=='{')-(source[at]=='}');at+=1
    return source[start:at]

class InterpolationMemoryTest(unittest.TestCase):
    def test_native_mode_is_lazy_and_failed_history_allocations_are_atomic(self):
        cc=os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        s=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
        init=block(s,'if (gsInterp < 0) {')
        allocation=block(s,'if (gsInterp > 0 && sInterpPrev == NULL) {')
        code='''
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#define MAX_VU_UNIFORMS 8
#define MAX_VERTS 32
#define INTERP_MAX_PRIMS 16
typedef int Vu0Uniform;
typedef int Vtx;
typedef int InterpPrim;
static void *sInterpPrev,*sInterpBlend,*sInterpPrevSig,*sInterpSig,*sInterpPrevVerts,*sInterpBlendVerts,*sInterpPrevPrims,*sInterpPrims,*sInterpPrimUsed;
static int gsInterp=-1,choice=0,calls,live,fail;
static void *allocate(size_t bytes){void *p;if(++calls==fail)return NULL;p=malloc(bytes);if(p)++live;return p;}
static void release(void *p){if(p)--live;free(p);}
static int Port_Setting(const char *key,int fallback){(void)key;(void)fallback;return choice;}
#define malloc allocate
#define free release
static void tick(void){
'''+init+'\n'+allocation+'''
}
static void reset(void){
    free(sInterpPrev);free(sInterpBlend);free(sInterpPrevSig);free(sInterpSig);free(sInterpPrevVerts);free(sInterpBlendVerts);free(sInterpPrevPrims);free(sInterpPrims);free(sInterpPrimUsed);
    sInterpPrev=sInterpBlend=sInterpPrevSig=sInterpSig=sInterpPrevVerts=sInterpBlendVerts=sInterpPrevPrims=sInterpPrims=sInterpPrimUsed=NULL;
    assert(live==0);gsInterp=-1;calls=0;
}
int main(void){
    tick();assert(gsInterp==0 && calls==0 && live==0);
    gsInterp=1;tick();assert(calls==9 && live==9);
    tick();assert(calls==9);reset();
    for(int i=1;i<=9;++i){fail=i;choice=1;tick();assert(gsInterp==0 && !sInterpPrev && live==0);reset();}
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='bt4-interpolation-memory-') as tmp:
            src=Path(tmp)/'probe.c';src.write_text(code)
            exe=Path(tmp)/('probe.exe' if os.name=='nt' else 'probe')
            prefix=['rtk','proxy'] if shutil.which('rtk') else []
            subprocess.run(prefix+[cc,'-std=c99','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True,capture_output=True)
            env={k:v for k,v in os.environ.items() if k!='BT3_INTERP'}
            subprocess.run(prefix+[str(exe)],env=env,check=True,capture_output=True)
