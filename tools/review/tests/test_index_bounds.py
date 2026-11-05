"""Check SIMD and portable bounds against the original pack matching loop."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[3]


class IndexBoundsTest(unittest.TestCase):
    def test_byte_and_nibble_parity(self):
        cc=os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "gs_index_bounds.h"
static void scalar(const uint8_t *p,int four,uint32_t *lo,uint32_t *hi){
    for(unsigned i=0;i<256;++i){
        uint32_t a=four?p[i]&15:p[i],b=p[i]>>4;
        if(a<*lo)*lo=a;if(a>*hi)*hi=a;
        if(four){if(b<*lo)*lo=b;if(b>*hi)*hi=b;}
    }
}
int main(void){
    uint8_t data[257];uint32_t seed=12345;
    for(unsigned n=0;n<12000;++n){
        for(unsigned i=0;i<257;++i){seed=seed*1664525u+1013904223u;data[i]=seed>>24;}
        if(n<256)for(unsigned i=0;i<257;++i)data[i]=(uint8_t)n;
        for(int four=0;four<2;++four){
            uint32_t lo=n%3?255:3,hi=n%3?0:9,a=lo,b=hi;
            scalar(data+(n&1),four,&a,&b);
            Gs_IndexBounds(data+(n&1),four,&lo,&hi);
            assert(a==lo&&b==hi);
        }
    }
    puts("Index bounds: 24000 byte/nibble, unaligned and accumulated ranges PASS");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);(d/'test.c').write_text(code,encoding='utf8')
            for name,flags in [('native',[]),('portable',['-U__SSE2__'])]:
                exe=d/(name+('.exe' if os.name=='nt' else ''))
                subprocess.run([cc,'-O2',*flags,'-I'+str(ROOT/'port/src/gs'),str(d/'test.c'),'-o',str(exe)],check=True)
                subprocess.run([str(exe)],check=True)


if __name__=='__main__':unittest.main()
