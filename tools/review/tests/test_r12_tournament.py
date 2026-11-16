"""Exercise the production bracket for every supported entrant count, without sockets."""
from pathlib import Path
import unittest
import test_r12_tag_crashes as probes
from test_tag_effect_lifecycle import function
ROOT=Path(__file__).resolve().parents[3]

class TournamentTests(unittest.TestCase):
    def test_byes_resolve_and_ui_distinguishes_them_from_pending_qualifiers(self):
        source=(ROOT/'port/src/gs/net_lobby.c').read_text(encoding='utf8')
        tour=source[source.index('typedef struct {\n    int state;'):source.index('} Tour;')+7]
        code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../port/src/gs/net_lobby.h"
typedef int s32;
#define TOUR LOBBY_TOUR
#define LEAVES LOBBY_TOUR_LEAVES
#define NAMEB LOBBY_NAME
#define NOTE 256
#define TOKEN 16
#define HOW_BYE 1
#define HOW_WALKOVER 2
#define HOW_PLAYED 3
#define SDL_snprintf snprintf
static void SDL_strlcpy(char *d,const char *s,int n){snprintf(d,n,"%s",s);}
'''+tour+r'''
static struct {int phase,mode,entrants,slot[LOBBY_SLOTS],dirty;Tour t;struct {unsigned id;char name[NAMEB];} m[LOBBY_SLOTS];} sL;
static unsigned seed=123456;
static unsigned os_random32(void){return ++seed;}
static void note_set(const char *s){}
static void t_log(void){}
static void t_tokens(void){}
static void tok_draw(int n){}
'''
        for name in ('t_matches','t_round','t_part','t_next','t_resolve','t_new'):
            code+=function('port/src/gs/net_lobby.c',name)+'\n'
        code+=r'''
static PortLobbyView sTbLv;
static int pageIndex;
static int Tb_BrPageIndex(void){return pageIndex;}
static int Tb_BrRoundsFor(const PortLobbyView *v){int r=0;for(int n=v->tourSize;n>1;n>>=1)++r;return r;}
'''+function('src/menu/port_tbo.c','Tb_BrPageView')+r'''
int main(void){
    for(int n=2;n<=TOUR;++n)for(int choice=0;choice<2;++choice){
        memset(&sL,0,sizeof(sL));sL.mode=1;sL.entrants=n;
        for(int i=0;i<LOBBY_SLOTS;++i){sL.slot[i]=i<n?i:-1;sL.m[i].id=i+1;snprintf(sL.m[i].name,NAMEB,"Player%d",i);}
        t_new();int played=0,byes=0,seen[TOUR]={0};
        for(int i=0;i<sL.t.size;++i)if(sL.t.leaf[i]>=0)++seen[sL.t.leaf[i]];else ++byes;
        for(int i=0;i<n;++i)assert(seen[i]==1);
        assert(byes==sL.t.size-n);
        for(int i=0;i<sL.t.size/2;++i)if(t_part(i,0)<0||t_part(i,1)<0){
            assert(sL.t.how[i]==HOW_BYE&&sL.t.win[i]>=0);
        }
        memset(&sTbLv,0,sizeof(sTbLv));sTbLv.tourSize=sL.t.size;
        memcpy(sTbLv.tourLeaf,sL.t.leaf,sizeof(sTbLv.tourLeaf));
        for(int i=0;i<LEAVES-1;++i)sTbLv.tourWin[i]=-1;
        int sections=sL.t.size>8?sL.t.size/8:1;
        for(pageIndex=0;pageIndex<sections;++pageIndex){
            PortLobbyView view;Tb_BrPageView(&view);
            for(int i=0;i<view.tourSize;++i){int original=sTbLv.tourLeaf[pageIndex*8+i];assert(view.tourLeaf[i]==(original<0?-2:original));}
        }
        if(sL.t.size>8){PortLobbyView view;pageIndex=sections;Tb_BrPageView(&view);for(int i=0;i<view.tourSize;++i)assert(view.tourLeaf[i]==-1);}
        while(sL.t.state==1){
            int m=sL.t.cur;assert(m>=0&&played<n);
            int a=t_part(m,0),b=t_part(m,1);assert(a>=0&&b>=0&&a!=b);
            sL.t.win[m]=choice?b:a;sL.t.how[m]=HOW_PLAYED;++played;t_resolve();
        }
        assert(played==n-1&&sL.t.champ>=0&&sL.t.champ<n);
    }
    puts("Tournament: 2..22 entrants, automatic byes, both winners and all UI pages PASS");
}
'''
        probes.ReportedTagCrashes.native(self,code)

if __name__=='__main__':unittest.main()
