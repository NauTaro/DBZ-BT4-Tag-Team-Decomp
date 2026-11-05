"""Exercise the renderer's eviction policy and its real address-index rebuild."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[3]

def function(source,name):
    start=source.index(name);at=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[at]=='{')-(source[at]=='}');at+=1
    return source[start:at]

class TextureBudgetTest(unittest.TestCase):
    def test_active_frame_safety_and_compacted_aliases(self):
        cc=os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        s=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
        code='''
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef unsigned GsTex;
#define MAX_TEX 8192
#define TEX_BUCKETS 32768
static int gsTexCount;
static unsigned gGsFrame=100,destroyed;
static void destroy(GsTex handle){assert(handle);++destroyed;}
static struct {void (*texDestroy)(GsTex);} backend={destroy},*sBackend=&backend;
'''
        code+=s[s.index('typedef struct Tex {'):s.index('/* ---- The look script')]
        for name in ('static uint32_t tex_bucket(', 'static void texture_cache_reindex(', 'static void texture_cache_trim(void) {'):
            code+=function(s,name)+'\n'
        code+='''
static void fill(unsigned mb,unsigned recent){
    memset(sTex,0,sizeof(sTex));destroyed=0;gsTexCount=5;
    for(unsigned i=0;i<5;++i){sTex[i].tex=i+1;sTex[i].tex0=i;sTex[i].bytes=(uint64_t)mb<<20;sTex[i].last=i<recent?99:40;}
    texture_cache_reindex();sLast=&sTex[4];sHdAlias[0].handle=5;sHdAlias[0].index=4;
}
int main(void){
    fill(32,2);texture_cache_trim();
    assert(gsTexCount==5 && !destroyed && sTextureBytes==(160ull<<20));
    assert(sHdAlias[0].handle==5); /* No work or churn while below budget. */
    fill(64,2);texture_cache_trim();
    assert(gsTexCount==2 && destroyed==3 && sTextureBytes==(128ull<<20));
    assert(!sLast && !sHdAlias[0].handle);
    for(unsigned i=0;i<2;++i){unsigned h=tex_bucket(sTex[i].tex0,sTex[i].texa,sTex[i].gen);assert(sTexBucket[h]==i+1);}
    fill(64,5);texture_cache_trim();
    assert(gsTexCount==5 && !destroyed && sTextureBytes==(320ull<<20));
    gGsFrame+=5;texture_cache_trim();
    assert(sTextureBytes<=(192ull<<20) && destroyed>0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='bt4-texture-budget-') as tmp:
            src=Path(tmp)/'probe.c';src.write_text(code)
            exe=Path(tmp)/('probe.exe' if os.name=='nt' else 'probe')
            prefix=['rtk','proxy'] if shutil.which('rtk') else []
            subprocess.run(prefix+[cc,'-std=c99','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True,capture_output=True)
            env={k:v for k,v in os.environ.items() if k!='BT3_TEXTURE_CACHE_MB'}
            subprocess.run(prefix+[str(exe)],env=env,check=True,capture_output=True)
