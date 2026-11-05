"""Partial HD allocations must not crash or queue incomplete GPU resources."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

class PackUploadTests(unittest.TestCase):
    def test_each_mip_allocation_and_gpu_creation_failure(self):
        cc=os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        s=(ROOT/'port/src/gs/gs_draw.c').read_text(encoding='utf8')
        a=s.index('static GsTex pack_texture(');b=s.index('\n}\n',a)+3
        code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gs_texpack.h"
typedef unsigned GsTex;
static unsigned allocations,frees,uploads,created,failAt,failCreate;
static void *reserve(size_t bytes){++allocations;return allocations==failAt?NULL:malloc(bytes);}
static void release(void *p){assert(p);++frees;free(p);}
static GsTex create(uint32_t w,uint32_t h,int format,int levels){
    assert(w==16&&h==8&&format==3&&levels==5);++created;return failCreate?0:123;
}
static int upload(GsTex t,int level,uint32_t w,uint32_t h,int fmt,const void *p,uint32_t bytes){
    assert(t==123&&fmt==3&&p&&bytes==16);
    assert(w==(16u>>level?16u>>level:1));assert(h==(8u>>level?8u>>level:1));
    for(unsigned i=0;i<bytes;++i)assert(((const unsigned char *)p)[i]==level);
    ++uploads;release((void *)p);return 1;
}
static struct {GsTex (*texCreate)(uint32_t,uint32_t,int,int);int (*texUpload)(GsTex,int,uint32_t,uint32_t,int,const void *,uint32_t);} backend={create,upload},*sBackend=&backend;
#define malloc reserve
#define free release
'''+s[a:b]+r'''
#undef malloc
#undef free
int main(void){
    TexPackImage p={0};uint8_t pixels[5][16];p.w=16;p.h=8;p.format=3;p.levels=5;
    for(int i=0;i<5;++i){memset(pixels[i],i,16);p.data[i]=pixels[i];p.bytes[i]=16;}
    for(failAt=1;failAt<=5;++failAt){
        allocations=frees=uploads=created=0;
        assert(!pack_texture(&p)&&allocations==failAt&&frees==failAt-1&&!uploads&&!created);
    }
    failAt=0;failCreate=1;allocations=frees=uploads=created=0;
    assert(!pack_texture(&p)&&allocations==5&&frees==5&&!uploads&&created==1);
    failCreate=0;allocations=frees=uploads=created=0;
    assert(pack_texture(&p)==123&&allocations==5&&frees==5&&uploads==5&&created==1);
    p.levels=TEXPACK_MAX_LEVELS+1;assert(!pack_texture(&p));
    puts("HD upload: all mip allocation failures, device allocation failure and successful ownership transfer PASS");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);(d/'probe.c').write_text(code)
            exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-O2','-I'+str(ROOT/'port/src/gs'),str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__=='__main__':unittest.main()
