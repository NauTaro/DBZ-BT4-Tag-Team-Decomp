"""Synthetic packet impairment must not activate in an ordinary launch."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

def selector_function():
    source=(ROOT/'port/src/gs/net.c').read_text(encoding='utf8')
    start=source.index('static void net_test_line(')
    brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

class NetworkDiagnosticTests(unittest.TestCase):
    def test_packet_impairment_requires_explicit_test_mode(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        code='''#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int sLoss=8,sLagMs=90,sLatencyMs=20;
'''+selector_function()+'''
int main(int argc,char **argv){
    assert(argc==4);net_test_line();
    assert(sLoss==atoi(argv[1])&&sLagMs==atoi(argv[2])&&sLatencyMs==atoi(argv[3]));
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);src=root/'diagnostics.c';exe=root/('diagnostics.exe' if os.name=='nt' else 'diagnostics')
            src.write_text(code,encoding='utf8');flags={'creationflags':0x08000000} if os.name=='nt' else {}
            p=subprocess.run([cc,'-std=c99',str(src),'-o',str(exe)],capture_output=True,text=True,timeout=30,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            for mode,impairment,expected in [
                (None,True,(0,0,0)),('0',True,(0,0,0)),('',True,(0,0,0)),
                ('true',True,(0,0,0)),('1',True,(3,170,45)),('1',False,(0,0,0))]:
                env={k:v for k,v in os.environ.items() if k not in
                     {'BT4_RENDER_TEST','BT3_NET_LOSS','BT3_NET_LAG','BT3_NET_LATENCY'}}
                if mode is not None:env['BT4_RENDER_TEST']=mode
                if impairment:env.update(BT3_NET_LOSS='3',BT3_NET_LAG='170',BT3_NET_LATENCY='45')
                with self.subTest(mode=mode,impairment=impairment):
                    p=subprocess.run([str(exe),*map(str,expected)],env=env,capture_output=True,text=True,timeout=15,**flags)
                    self.assertEqual(p.returncode,0,p.stdout+p.stderr)

if __name__=='__main__':unittest.main()
