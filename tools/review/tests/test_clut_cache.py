"""Exercise the actual renderer cache, not just the palette helper."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


class ClutCacheTest(unittest.TestCase):
    def test_displayed_colours_and_upload_generations(self):
        cc = os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
        if not cc:
            self.skipTest('A C compiler is required')
        source = (ROOT / 'port/src/gs/gs_draw.c').read_text(encoding='utf8')
        start = source.index('static uint32_t clut_hash(')
        end = source.index('\n}\n', start) + 3
        code = '''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
static uint32_t palette[16][16],gGsPageGen[512],reads;
static uint32_t Gs_VramRead(uint32_t bp,uint32_t bw,uint32_t psm,uint32_t x,uint32_t y){
    assert(bp==0 && bw==1 && psm==0 && x<16 && y<16);
    ++reads;return palette[y][x];
}
'''+source[start:end]+'''
int main(void){
    uint32_t x,y,i,base,before;
    for(y=0;y<16;++y)for(x=0;x<16;++x)palette[y][x]=0x80000000u+y*16+x;
    base=clut_hash(0,0,16);before=reads;
    assert(clut_hash(0,0,16)==base && reads==before);
    for(i=0;i<16;++i){
        x=i&7;y=i>>3;palette[y][x]^=0x00ff0000;++gGsPageGen[0];
        assert(clut_hash(0,0,16)!=base);
        palette[y][x]^=0x00ff0000;++gGsPageGen[0];
        assert(clut_hash(0,0,16)==base);
    }
    for(x=8;x<16;++x){
        palette[0][x]^=0x00ff0000;++gGsPageGen[0];
        assert(clut_hash(0,0,16)==base);
    }
    base=clut_hash(0,0,256);
    for(i=0;i<256;++i){
        x=i&15;y=i>>4;palette[y][x]^=0x0000ff00;++gGsPageGen[0];
        assert(clut_hash(0,0,256)!=base);
        palette[y][x]^=0x0000ff00;++gGsPageGen[0];
        assert(clut_hash(0,0,256)==base);
    }
    puts("Renderer CLUT: all displayed colours invalidate, neighbours do not, unchanged uploads stay cached PASS");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            (path/'probe.c').write_text(code, encoding='utf8')
            exe = path/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-O2',str(path/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)


if __name__ == '__main__':
    unittest.main()
