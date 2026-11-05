"""Tag actors must not overwrite adjacent water particle lists."""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[3]

class WaterWakeTests(unittest.TestCase):
    def test_all_ten_actor_slots_and_invalid_object_ids(self):
        cc=os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        s=(ROOT/'src/battle/eft_e.c').read_text(encoding='utf8')
        start=s.index('#ifdef PORT\n/* Object IDs of Tag Team fighters')
        end=s.index('/* Spray along a technique',start)
        code='''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define PORT 1
typedef int32_t s32;typedef uint8_t u8;
typedef struct {s32 objId;uint32_t frame;s32 on;} EftWaterWake;
static int actors=2,split;
int TagBattle_Count(void){return actors;}
int Battle_IsSplitScreen(void){return split;}
static struct {EftWaterWake *wake;} dust,*gEftDust=&dust;
'''+s[start:end]+'''
int main(void){
    struct {uint32_t before;EftWaterWake slots[EFT_WATER_WAKE_CAPACITY];uint32_t after;} memory={0};
    memory.before=0x12345678;memory.after=0xabcdef12;dust.wake=memory.slots;
    for(actors=2;actors<=10;actors+=2){
        for(int i=0;i<actors;++i){
            assert(EftWater_SetWake(i,0));
            assert(memory.slots[i].on==1&&memory.slots[i].objId==i);
            assert(EftWater_SetWake(i,1)&&!memory.slots[i].on);
        }
        assert(!EftWater_SetWake(-1,0));
        assert(!EftWater_SetWake(actors,0));
        assert(!EftWater_SetWake(1000,0));
        assert(memory.before==0x12345678&&memory.after==0xabcdef12);
    }
    split=1;assert(!EftWater_SetWake(0,0));split=0;
    gEftDust=0;assert(!EftWater_SetWake(0,0));
    puts("Water wakes: 2/4/6/8/10 actors, adjacent canaries and invalid IDs PASS");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);(d/'probe.c').write_text(code)
            exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-O2',str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__=='__main__':unittest.main()
