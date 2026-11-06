"""Verify route authority with the real release verifier and signed adversarial inputs."""
from pathlib import Path
import importlib.util,json,os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
class OnlineRouteTests(unittest.TestCase):
 def test_signed_policy_expiry_replay_targets_and_room_namespaces(self):
  cc=shutil.which('gcc');cxx=shutil.which('g++');sdk=os.environ.get('SDL3_DIR')
  if not cc or not cxx or not sdk:self.skipTest('C/C++ and SDL3 SDK required')
  spec=importlib.util.spec_from_file_location('route_signing',ROOT/'port/tools/update_key.py')
  key=importlib.util.module_from_spec(spec);spec.loader.exec_module(key)
  seed=bytes(range(32));pub=key.public_key(seed).hex()
  body=dict(schema=1,epoch=5,issued_at=10000,expires_at=10600,capability=1,active='secondary',minimum='0.1.39.5-R3',require_update=False)
  def signed(b):
   payload=json.dumps(b,separators=(',',':'));sig=key.sign(seed,b'BT4-ONLINE-ROUTE-V1\n'+payload.encode()).hex()
   return json.dumps(dict(payload=payload,signature=sig),separators=(',',':'))
  entries=[signed(body)]
  for overrides in ({'schema':2},{'active':'https://attacker.invalid'},{'epoch':0},{'epoch':2147483648},
                    {'issued_at':10301},{'expires_at':10000},{'expires_at':100000},{'minimum':'Dev'},
                    {'capability':0},{'capability':101},{'unexpected':True}):entries.append(signed(body|overrides))
  bad=json.loads(entries[0]);bad['signature']='00'*64;entries.append(json.dumps(bad))
  bad=json.loads(entries[0]);bad['payload']=bad['payload'].replace('secondary','primary');entries.append(json.dumps(bad))
  entries.extend(['{}','x'*8193])
  code='''#include <cassert>
#define JSON_NOEXCEPTION
#include "JSON_HEADER"
#include "ROUTE_HEADER"
using J=nlohmann::json;
int main(){bt4route::Policy before,p;
 const char *cases[]={CASES};
 assert(bt4route::decode<J>(cases[0],10000,before,p));assert(p.active==1&&p.epoch==5&&!p.needsUpdate());
 for(unsigned i=1;i<sizeof(cases)/sizeof(cases[0]);i++){bt4route::Policy rejected;assert(!bt4route::decode<J>(cases[i],10000,before,rejected));}
 bt4route::Policy next;assert(!bt4route::decode<J>(cases[0],10600,before,next));
 before=p;before.epoch=6;assert(!bt4route::decode<J>(cases[0],10000,before,next));
 before=p;before.active=0;assert(!bt4route::decode<J>(cases[0],10000,before,next));
 before=p;before.issued=10001;assert(!bt4route::decode<J>(cases[0],10000,before,next));
 before=p;before.minimum="0.1.39.5-R4";assert(!bt4route::decode<J>(cases[0],10000,before,next));
 assert(p.fresh(10599)&&!p.fresh(10600));p.requireUpdate=true;p.minimum="0.1.39.5-R5";assert(p.needsUpdate());
 p.minimum="0.1.39.5-R3";assert(!p.needsUpdate());p.capability=2;assert(p.needsUpdate());
 assert(bt4route::codeProject("0ABCDE")==1);assert(bt4route::codeProject("1ABCDE")==0);assert(bt4route::codeProject("ABCDEF")==0);
 assert(bt4route::admission("create")&&bt4route::admission("join")&&bt4route::admission("queue")&&bt4route::admission("list"));
 assert(!bt4route::admission("signal")&&!bt4route::admission("leave")&&!bt4route::admission("lease"));
}'''.replace('JSON_HEADER',(ROOT/'port/third_party/voice/json.hpp').as_posix()).replace('ROUTE_HEADER',(ROOT/'port/src/gs/online_route.h').as_posix()).replace('CASES',','.join(json.dumps(x) for x in entries))
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'route.cpp').write_text(code,encoding='utf8')
   flags={'creationflags':0x08000000} if os.name=='nt' else {}
   env=os.environ.copy();env['PATH']=str(Path(sdk)/'bin')+os.pathsep+env['PATH']
   objs=[]
   for name in ('update_sign','update_crypto','update_monocypher'):
    obj=p/(name+'.o');objs.append(str(obj))
    r=subprocess.run([cc,'-O2','-I'+str(Path(sdk)/'include'),'-c',str(ROOT/f'port/src/gs/{name}.c'),'-o',str(obj)],capture_output=True,text=True,timeout=60,**flags)
    self.assertEqual(r.returncode,0,r.stdout+r.stderr)
   exe=p/('route.exe' if os.name=='nt' else 'route')
   r=subprocess.run([cxx,'-std=c++17','-O2',f'-DPORT_UPDATE_TEST_KEY="{pub}"',str(p/'route.cpp'),*objs,'-L'+str(Path(sdk)/'lib'),'-lSDL3','-o',str(exe)],capture_output=True,text=True,timeout=90,**flags)
   self.assertEqual(r.returncode,0,r.stdout+r.stderr)
   r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15,env=env,**flags);self.assertEqual(r.returncode,0,r.stdout+r.stderr)

if __name__=='__main__':unittest.main()
