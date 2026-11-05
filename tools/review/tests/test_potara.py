"""Exercise the production item reader and battle setup, including both online transports.

BT4_POTARA_TABLE optionally supplies common file 4 from a local installation.
No ISO data is included in this test or written back to the installation.
"""
from pathlib import Path
import os, shutil, struct, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[3]

def function(source, signature):
    start=source.index(signature); end=source.index('{',start)+1; depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

class PotaraTests(unittest.TestCase):
    def test_tag_draft_keeps_each_local_set_and_rejects_network_items(self):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('A C compiler is required')
        source=(ROOT/'src/battle/battle_work.c').read_text()
        code=r'''
#include "common.h"
#include "battle/battle_setup.h"
#include <assert.h>
#include <stdio.h>
extern void *memset(void *,s32,u32);
static BattleItemSet sTagDraftItems[10];static s32 sTagDraftItemChara[10];
static int network;
int Port_TagNetwork(void){return network;}
'''+ '\n'.join(function(source,s) for s in ('void TagBattle_ClearDraftItems(', 'void TagBattle_SetDraftItems(', 'BattleItemSet *TagBattle_GetDraftItems('))+r'''
int main(void){
    u16 ids[8]={134,114,1,350,65535,351,133,0};
    TagBattle_ClearDraftItems();
    for(int i=0;i<10;++i)assert(!TagBattle_GetDraftItems(i,i));
    TagBattle_SetDraftItems(-1,0,ids);TagBattle_SetDraftItems(10,0,ids);
    for(int i=0;i<10;++i){ids[1]=114+i;TagBattle_SetDraftItems(i,30+i,ids);}
    for(int pass=0;pass<2;++pass){network=pass;
        for(int i=0;i<10;++i){BattleItemSet *set=TagBattle_GetDraftItems(i,30+i);
            assert(!TagBattle_GetDraftItems(i,99));
            if(pass){assert(!set);continue;}
            assert(set&&set->id[0]==134&&set->id[1]==114+i&&set->id[2]==1&&set->id[3]==350);
            assert(!set->id[4]&&!set->id[5]&&set->id[6]==133&&!set->id[7]);
        }
    }
    network=0;TagBattle_ClearDraftItems();for(int i=0;i<10;++i)assert(!TagBattle_GetDraftItems(i,30+i));
    puts("Tag Potara: ten independent local sets, changed fighters, reset and online isolation PASS");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);(d/'probe.c').write_text(code);exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99','-fno-builtin','-DPORT','-I'+str(ROOT/'include'),'-I'+str(ROOT/'port/include'),str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_all_items_offline_and_online_balance(self):
        cc=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc')
        if not cc:self.skipTest('A C compiler is required')
        supplied=os.environ.get('BT4_POTARA_TABLE')
        if supplied:data=Path(supplied).read_bytes()
        else:
            data=bytearray(32+250*60+350*40)
            struct.pack_into('<4I',data,0,2,32,15032,len(data))
            for i in range(250):struct.pack_into('<i',data,32+i*60,i%7+1)
            for i in range(350):
                at=15032+i*40;data[at]=2 if 135<=i<150 else 0
                struct.pack_into('<4h',data,at+12,i%17,-i%11,i%5,i%9)
                struct.pack_into('<4I',data,at+24,1<<(i%32),1<<((i+2)%32),1<<((i+4)%32),1<<((i+6)%32))
            for i in range(113,124):
                bit=0x4D+i-113 if i<123 else 0x74
                struct.pack_into('<4I',data,15032+i*40+24,1<<0x16,0,(1<<(bit%32)) if bit<96 else 0,(1<<(bit%32)) if bit>=96 else 0)
            for i,bit in ((132,122),(133,123)):
                struct.pack_into('<4h',data,15032+i*40+12,0,0,0,0)
                struct.pack_into('<4I',data,15032+i*40+24,0,0,0,1<<(bit-96))
        source=(ROOT/'src/battle/battle_load.c').read_text()
        setup=SETUP_PREFIX+'\n'.join(function(source,s) for s in ('void BtlMember_ApplyItems(', 'void BtlMember_Init('))
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);(d/'table.bin').write_bytes(data);(d/'setup.c').write_text(setup);(d/'probe.c').write_text(PROBE)
            exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99','-O1','-fno-builtin','-ffunction-sections','-fdata-sections','-DPORT','-I'+str(ROOT/'include'),'-I'+str(ROOT/'port/include'),str(d/'probe.c'),str(d/'setup.c'),str(ROOT/'src/battle/view_b_b.c'),'-Wl,--gc-sections','-o',str(exe)],check=True)
            subprocess.run([str(exe),str(d/'table.bin')],check=True)

SETUP_PREFIX=r'''
#include "common.h"
#include "battle/battle_setup.h"
#include "battle/bt4_potara.h"
#include "sys/save.h"
extern void *memset(void *,s32,u32);
extern void ItemSet_GetStats(BattleItemSet *,s32 *,s32 *,s32);
'''
PROBE=r'''
#include "common.h"
#include "battle/view_b.h"
#include "battle/battle_setup.h"
#include "sys/common.h"
#include "sys/save.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
extern void *memset(void *,s32,u32);
extern int memcmp(const void *,const void *,size_t);
CommonRes *gCommonRes;SaveData *gSaveData;
static int nativeOnline,tagOnline;
int Port_NetSession(void){return nativeOnline;}
int Port_TagNetwork(void){return tagOnline;}
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);unsigned char *raw=malloc(size);assert(raw&&fread(raw,1,size,f)==size);fclose(f);
    CommonRes common={0};gCommonRes=&common;common.data[2]=raw;
    ChrTblFile *hdr=(void*)raw;ItemTblEntry *table=(void*)(raw+(hdr->itemOffset&~3));
    ChrTblEntry *chars=(void*)(raw+(hdr->charaOffset&~3));
    assert(sizeof(ItemTblEntry)==40&&sizeof(ChrTblEntry)==60);
    for(int transport=0;transport<3;++transport){
        nativeOnline=transport==1;tagOnline=transport==2;
        for(int id=1;id<=350;++id){
            BattleMember m;BattleItemSet items={{0}};items.id[0]=id;
            BtlMember_Init(&m,86,2,1,13,75.0f,&items);
            assert(m.chara==86&&m.costume==2&&m.variant==1&&m.health==75.0f);
            int stats[4]={0},bits[4]={0},ai=chars[86].aiType;
            if(!transport){
                ItemTblEntry *it=&table[id-1];
                if(it->type==ITEMTBL_TYPE_AI){if(id-ITEMTBL_AI_FIRST)ai=id-ITEMTBL_AI_FIRST;}
                else for(int k=0;k<4;++k){stats[k]=it->stat[k];bits[k]=it->ability[k];}
                assert(m.items.id[0]==id);
            }else if((id>=114&&id<=124)||id==133||id==134){
                bits[2]=table[id-1].ability[2]&0x007FE000u;bits[3]=table[id-1].ability[3]&0x0C100000u;
                assert(m.items.id[0]==id);
            }else assert(m.items.id[0]==0);
            int expected[8]={0,stats[2],stats[0],stats[1],stats[2],stats[3],stats[3],stats[3]};
            assert(!memcmp(m.bonus,expected,sizeof(expected))&&!memcmp(m.ability,bits,sizeof(bits))&&m.aiType==ai);
        }
        /* Stacking across all eight entries must survive offline and be
           mechanically neutral on both network paths, including red items. */
        BattleItemSet mixed={{1,2,3,114,115,124,350,0}};BattleMember m;
        BtlMember_Init(&m,107,1,0,13,100.0f,&mixed);
        if(transport){for(int k=0;k<8;++k)assert(!m.bonus[k]);assert(!m.ability[0]&&!m.ability[1]);
            assert(!(m.ability[2]&~0x007FE000u)&&!(m.ability[3]&~0x0C100000u));}
        BattleItemSet invalid={{351,65535,0,0,0,0,0,0}};
        BtlMember_Init(&m,107,1,0,13,100.0f,&invalid);
        assert(!m.items.id[0]&&!m.items.id[1]);
        BattleItemSet clothes={{134,0,0,0,0,0,0,0}};
        BtlMember_Init(&m,86,2,0,13,100.0f,&clothes);
        assert(m.variant==1&&m.costume==2&&m.chara==86);
        for(int k=0;k<8;++k)assert(!m.bonus[k]);
        assert(!m.ability[0]&&!m.ability[1]&&!m.ability[2]&&m.ability[3]==(1<<27));
        clothes.id[0]=133;BtlMember_Init(&m,86,2,0,13,100.0f,&clothes);
        assert(m.variant==0&&m.ability[3]==(1<<26));
    }
    free(raw);puts("Potara: all 350 original entries, stacked items, character AI and online 1v1/Tag visual-only balance PASS");
}
'''
if __name__=='__main__':unittest.main()
