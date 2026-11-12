"""Compile the production authentication functions against deterministic HTTP replies.

No network calls, real credentials, game windows or persisted player data.
Run with CXX pointing at a C++17 compiler when it is not on PATH.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SessionRecoveryTests(unittest.TestCase):
    def test_recovery_and_identity_preservation(self):
        source = (ROOT / 'port/src/gs/cloudflare_signal.inc').read_text(encoding='utf-8')
        auth = source[source.index('static bool cloudStoreSession('):source.index('static int cloudEntry(')]
        header = (ROOT / 'port/third_party/voice/json.hpp').as_posix()
        fixture = r'''
#include <array>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "JSON_HEADER"
using Sj=nlohmann::json;
using Uint64=unsigned long long;
struct Identity {std::string access,refresh,user,file;Uint64 renewAt=0;};
static std::array<Identity,3> identities;
static int activeProject=0,saves=0;
static long lastHttpStatus=0;
static const char *cloudHost="fixture.invalid";
static std::string str(const Sj &j,const char *k){auto i=j.find(k);return i!=j.end()&&i->is_string()?i->get<std::string>():std::string();}
static Uint64 SDL_GetTicks(){return 100;}
static bool identitySlotFor(Identity &,const std::string &){return true;}
static bool identitySlot(){return true;}
static Identity &currentIdentity(){return identities[activeProject];}
static void saveIdentityFor(Identity &){++saves;}
static void saveIdentity(){++saves;}
struct Reply {std::string path;int project;long code;Sj body;};
static std::vector<Reply> replies;
static size_t consumed;
static bool http(const std::string &path,const Sj &,bool,Sj &answer,bool=false,const char *=nullptr){
 assert(consumed<replies.size());const auto &r=replies[consumed++];
 assert(path==r.path);assert(activeProject==r.project);lastHttpStatus=r.code;answer=r.body;return r.code>=200&&r.code<300;
}
PRODUCTION_AUTH
static const std::string uid="11111111-1111-4111-8111-111111111111";
static Sj session(const std::string &id=uid){return {{"access_token","cloud-access"},{"refresh_token","cloud-new"},{"user",{{"id",id}}}};}
static Sj legacy(const std::string &id=uid){return {{"access_token","legacy-access"},{"refresh_token","legacy-new"},{"user",{{"id",id}}}};}
static Sj rejected(){return {{"error","AUTH_REQUIRED"}};}
static void reset(){identities={};activeProject=0;lastHttpStatus=0;saves=0;replies.clear();consumed=0;}
static void done(const char *name){assert(consumed==replies.size());assert(activeProject==0);std::cout<<name<<": PASS\n";}
int main(){
 reset();identities[2].refresh="old";replies={{"/v5/auth/refresh",0,200,session()}};
 assert(cloudAuthenticate());assert(identities[2].user==uid);done("healthy cloud renewal");

 reset();identities[2].refresh="stale";identities[1].refresh="legacy";
 replies={{"/v5/auth/refresh",0,401,rejected()},{"/auth/v1/token?grant_type=refresh_token",1,200,legacy()},{"/v5/auth/exchange",1,200,session()}};
 assert(cloudAuthenticate());assert(identities[2].user==uid);assert(identities[2].refresh=="cloud-new");done("rejected cloud token recovers original account");

 reset();identities[2].refresh="keep";identities[1].refresh="legacy";replies={{"/v5/auth/refresh",0,503,Sj::object()}};
 assert(!cloudAuthenticate());assert(identities[2].refresh=="keep");assert(!saves);done("transport failure keeps credentials");

 reset();identities[2].refresh="keep";replies={{"/v5/auth/refresh",0,401,rejected()}};
 assert(!cloudAuthenticate());assert(identities[2].refresh=="keep");assert(!saves);done("no replacement anonymous account");

 reset();identities[1].refresh="expired";identities[0].refresh="valid-primary";
 replies={{"/auth/v1/token?grant_type=refresh_token",1,400,{{"error_code","refresh_token_not_found"}}},{"/auth/v1/token?grant_type=refresh_token",0,200,legacy()},{"/v5/auth/exchange",0,200,session()}};
 assert(cloudAuthenticate());assert(identities[2].user==uid);done("stale secondary does not mask primary");

 reset();identities[2].refresh="stale";identities[2].user="22222222-2222-4222-8222-222222222222";identities[1].refresh="other-account";
 replies={{"/v5/auth/refresh",0,401,rejected()},{"/auth/v1/token?grant_type=refresh_token",1,200,legacy()}};
 assert(!cloudAuthenticate());assert(identities[2].refresh=="stale");assert(identities[2].user!=uid);done("established account cannot be replaced");

 reset();identities[2].refresh="stale";identities[2].user=uid;identities[1].refresh="legacy";
 replies={{"/v5/auth/refresh",0,401,rejected()},{"/auth/v1/token?grant_type=refresh_token",1,200,legacy()},{"/v5/auth/exchange",1,200,session("22222222-2222-4222-8222-222222222222")}};
 assert(!cloudAuthenticate());assert(identities[2].refresh=="stale");assert(identities[2].user==uid);done("exchange validates returned account");

 reset();replies={{"/v5/auth/start",0,200,session()}};
 assert(cloudAuthenticate());done("fresh installation can register");

 reset();identities[1].refresh="retained";replies={{"/auth/v1/token?grant_type=refresh_token",1,0,Sj()}};
 assert(!cloudAuthenticate());assert(identities[1].refresh=="retained");done("legacy outage preserves identity");
}
'''.replace('"JSON_HEADER"', '"' + header + '"').replace('PRODUCTION_AUTH', auth).replace('const char *=nullptr','const char *host=nullptr')
        with tempfile.TemporaryDirectory(prefix='bt4-session-test-') as temp:
            path=Path(temp)
            cpp=path/'session.cpp';cpp.write_text(fixture,encoding='utf-8')
            exe=path/('session.exe' if os.name=='nt' else 'session')
            flags={'creationflags':0x08004000} if os.name=='nt' else {}
            compile_result=subprocess.run([os.environ.get('CXX','g++'),'-std=c++17','-O1',str(cpp),'-o',str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(compile_result.returncode,0,compile_result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(result.returncode,0,result.stderr+result.stdout)
            self.assertEqual(result.stdout.count(': PASS'),9)
            print(result.stdout)


if __name__=='__main__':
    unittest.main()
