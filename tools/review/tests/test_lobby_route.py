"""Exercise the production lobby-route parser with isolated transport and clock stubs."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]
class LobbyRouteTests(unittest.TestCase):
    def test_fallback_and_match_packet_validation(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        code=r"""#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LOBBY_SLOTS 8
#define SEC 1000000000ULL
#define MAGIC 0x4e335442
struct sockaddr_in {unsigned ip,port;};
static struct {
 int on,leaving,phase,players[2],routeViaOwner;
 uint32_t routePeerId,inGen,myId,gen;
 uint64_t goNs;
 struct sockaddr_in routePeer,owner;
 struct {int on;uint32_t id;struct sockaddr_in addr;} m[LOBBY_SLOTS];
} sL;
static uint64_t clock_ns;
static int sends,connected,bytes;
static uint8_t saved[4096];static struct sockaddr_in dest;
static uint64_t now_ns(void){return clock_ns;}
static int NetIo_Connected(void){return connected;}
static int same(const struct sockaddr_in *a,const struct sockaddr_in *b){return a->ip==b->ip&&a->port==b->port;}
static uint32_t get(const void *p,int i){uint32_t v;memcpy(&v,(const uint8_t*)p+4*i,4);return v;}
static void put(void *p,int i,uint32_t v){memcpy((uint8_t*)p+4*i,&v,4);}
static void NetIo_Send(const void *p,int n,const struct sockaddr_in *to){++sends;bytes=n;memcpy(saved,p,(size_t)n);dest=*to;}
"""+(ROOT/'port/src/gs/net_lobby_route.inc').read_text(encoding='utf8')+r"""
int main(void){
 uint8_t p[ROUTE_HEADER+ROUTE_MAX+1]={0},body[8];struct sockaddr_in from;
 put(body,0,MAGIC);put(body,1,1);
 sL.on=2;sL.inGen=9;sL.myId=101;sL.routePeerId=102;
 sL.routePeer.port=222;sL.owner.port=444;clock_ns=SEC;
 assert(!Lobby_RouteSend(body,8,&sL.routePeer)&&sends==0);
 clock_ns=3*SEC;connected=1;assert(!Lobby_RouteSend(body,8,&sL.routePeer));
 connected=0;assert(Lobby_RouteSend(body,8,&sL.routePeer)==1);
 assert(sends==1&&bytes==28&&dest.port==444&&get(saved,3)==101&&get(saved,4)==102);
 memcpy(p,saved,28);sends=0;memset(&sL,0,sizeof(sL));
 sL.on=1;sL.phase=1;sL.gen=9;sL.players[0]=1;sL.players[1]=2;
 sL.m[1].on=sL.m[2].on=1;sL.m[1].id=101;sL.m[2].id=102;
 sL.m[1].addr.port=111;sL.m[2].addr.port=222;from=sL.m[1].addr;
 assert(Lobby_RouteRecv(p,28,&from)==0&&sends==1&&dest.port==222);
 put(p,2,8);assert(!Lobby_RouteRecv(p,28,&from)&&sends==1);put(p,2,9);
 from.port=333;assert(!Lobby_RouteRecv(p,28,&from)&&sends==1);from.port=111;
 put(p,4,999);assert(!Lobby_RouteRecv(p,28,&from)&&sends==1);put(p,4,102);
 sL.m[2].on=0;assert(!Lobby_RouteRecv(p,28,&from)&&sends==1);sL.m[2].on=1;
 put(p+20,1,T_L_ROUTE);assert(!Lobby_RouteRecv(p,28,&from)&&sends==1);put(p+20,1,1);
 assert(!Lobby_RouteRecv(p,27,&from)&&sends==1);
 assert(!Lobby_RouteRecv(p,sizeof(p),&from)&&sends==1);
 memset(&sL,0,sizeof(sL));sL.on=2;sL.inGen=9;sL.myId=102;sL.routePeerId=101;
 sL.owner.port=444;sL.routePeer.port=111;from.port=555;
 assert(!Lobby_RouteRecv(p,28,&from));from=sL.owner;
 assert(Lobby_RouteRecv(p,28,&from)==8&&from.port==111&&get(p,1)==1&&sL.routeViaOwner);
 return 0;
}
"""
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);src=root/'route.c';exe=root/('route.exe' if os.name=='nt' else 'route')
            src.write_text(code,encoding='utf8');flags={'creationflags':0x08000000} if os.name=='nt' else {}
            p=subprocess.run([cc,'-std=c99',str(src),'-o',str(exe)],capture_output=True,text=True,timeout=30,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            env={k:v for k,v in os.environ.items() if not k.startswith('BT4_')}
            p=subprocess.run([str(exe)],env=env,capture_output=True,text=True,timeout=15,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
if __name__=='__main__':unittest.main()
