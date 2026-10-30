"""Order upstream engine versions and BT4 revisions, including the legacy bridge."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
class RevisionTests(unittest.TestCase):
    def test_release_order_and_legacy_migration(self):
        bundled_cc=ROOT.parent.parent/'build-tools/ucrt64/bin/gcc.exe'
        cc=os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc') or (str(bundled_cc) if bundled_cc.exists() else None)
        if not cc:self.skipTest('A C compiler is required')
        source=(ROOT/'port/src/gs/update_sign.c').read_text(encoding='utf8')
        functions=source[source.index('static int version_parts'):source.index('/* ---- SHA-256')]
        code='#include <assert.h>\n#include <stdio.h>\n#include <string.h>\n#define SDL_snprintf snprintf\n'+functions+r'''
int main(void){
 char label[64];int parts[4];
 assert(UpdateVer_Compare("0.1.39.1-R1","0.1.39.1-R2")==-1);
 assert(UpdateVer_Compare("0.1.39.1 R2","v0.1.39.1-R2")==0);
 assert(UpdateVer_Compare("0.1.39.2","v0.1.39.3")==-1);
 assert(UpdateVer_Compare("0.1.39.3","v0.1.39.1-R2")==0);
 assert(UpdateVer_Compare("0.1.39.3","v0.1.39.1-R3")==-1);
 assert(UpdateVer_Compare("0.1.39.3","v0.1.39.2-R3")==-1);
 assert(UpdateVer_Compare("0.1.39.1-R3","v0.1.39.2-R3")==-1);
 assert(UpdateVer_Compare("0.1.39.2-R3","v0.1.39.2-R3")==0);
 assert(UpdateVer_Compare("0.1.39.2-R3","v0.1.39.2-R4")==-1);
 assert(UpdateVer_Compare("0.1.39.2-R4","v0.1.39.2-R4")==0);
 assert(UpdateVer_Compare("0.1.39.1-R99","v0.1.39.2-R1")==-1);
 assert(UpdateVer_Compare("0.1.39.1-R99","v0.1.40-R1")==-1);
 assert(UpdateVer_Compare("dev","0.1.39.1-R1")==-2);
 assert(!UpdateVer_Parse("0.1.39.1-R",parts));
 assert(!UpdateVer_Parse("0.1.39.1-R2extra",parts));
 assert(!UpdateVer_Parse("0.1.39-rc1",parts));
 assert(!UpdateVer_Parse("0.1.39.1.2",parts));
 UpdateVer_Display("0.1.39.3",label,sizeof(label));assert(!strcmp(label,"0.1.39.1 R2"));
 UpdateVer_Display("v0.1.39.1-R3",label,sizeof(label));assert(!strcmp(label,"0.1.39.1 R3"));
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/'revision.c';exe=Path(d)/('revision.exe' if os.name=='nt' else 'revision')
            src.write_text(code,encoding='utf8');flags={'creationflags':0x08000000} if os.name=='nt' else {}
            env=dict(os.environ)
            if Path(cc).exists():env['PATH']=str(Path(cc).parent)+os.pathsep+env.get('PATH','')
            p=subprocess.run([cc,'-std=c99',str(src),'-o',str(exe)],capture_output=True,text=True,timeout=30,env=env,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,env=env,**flags);self.assertEqual(p.returncode,0,p.stdout+p.stderr)
if __name__=='__main__':unittest.main()
