"""Drive the production tournament input through each selection stage."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[3]
def function(text,signature):
    start=text.index(signature);at=text.index('{',start)+1;depth=1
    while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
    return text[start:at]
class TournamentInputTests(unittest.TestCase):
    def test_bt4_tournament_controls_and_original_lock(self):
        cc=os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        source=(ROOT/'src/menu/menu_i_d.c').read_text()
        code=PREFIX+function(source,'static s32 EntrySel_SelectionLocked(void) {')+'\n'+function(source,'void EntrySel_Input(')+SUFFIX
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);(d/'probe.c').write_text(code)
            exe=d/('probe.exe' if os.name=='nt' else 'probe')
            subprocess.run([cc,'-std=gnu99','-fno-builtin','-DPORT','-I'+str(ROOT/'include'),'-I'+str(ROOT/'port/include'),str(d/'probe.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
PREFIX='#include "common.h"\n#include "menu/menu_i.h"\n#include "menu/menu_j.h"\n#include "sys/pad.h"\n#include "sys/save.h"\n#include <assert.h>\n#include <stdio.h>\nESel *gEntrySel;Pad gPad[PAD_COUNT];\n__typeof__(gProgress) gProgress;__typeof__(gSaveData) gSaveData;\n#define ES_CUR (gEntrySel->sel->entry[gEntrySel->sel->cur])\n#define ES_CELL (gEntrySel->grid[ES_CUR.row * ESEL_COLS + ES_CUR.col])\nstatic int moves;\nvoid ChrGrid_MoveLeft(ESelCell *g,s32 *col,s32 row){++moves;--*col;}\nvoid ChrGrid_MoveRight(ESelCell *g,s32 *col,s32 row){++moves;++*col;}\nvoid ChrGrid_MoveUp(ESelCell *g,s32 *col,s32 *row,s32 rows){++moves;--*row;}\nvoid ChrGrid_MoveDown(ESelCell *g,s32 *col,s32 *row,s32 rows){++moves;++*row;}\nvoid ChrGrid_PrevForm(s32 *g,s32 *form){++moves;--*form;}\nvoid ChrGrid_NextForm(s32 *g,s32 *form){++moves;++*form;}\ns32 ChrTbl_WrapCostume(s32 chara, s32 *costume){return 0;}\nvoid ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames){}\nvoid EntrySel_ChangeImage(void){}\nvoid EntrySel_ClipGoto(s32 movie, s32 kind, char *label){}\nvoid EntrySel_SetMemberTex(void){}\nvoid EntrySel_SetRowTex(void){}\nvoid EntrySel_SwapRowTex(void){}\nvoid Flash_GotoLabel(MFlash *flash, char *label, s32 fromStart){}\nvoid ItemHelp_Close(void){}\nvoid ItemHelp_Open(void){}\nvoid ItemPanel_Hide(s32 side){}\ns32 ItemPanel_Input(s32 side, s32 pad){return 0;}\nvoid ItemPanel_SetChara(s32 side, s32 chara, s32 slot, s32 set, s32 fromRec){}\nvoid ItemPanel_Show(s32 side){}\nvoid MsgWin_Close(void){}\nvoid MsgWin_Open(void){}\nu32 Rand_Range(u32 n){return 0;}\ns32 Snd_PlaySe(u32 mask, s32 id){return 0;}\n'
SUFFIX='int main(void){\n static ESel entry;static ESelState selection;static TourProgress progress;\n gEntrySel=&entry;entry.sel=&selection;gProgress=(void*)&progress;\n for(int tour=0;tour<5;++tour){\n  for(int step=0;step<=5;++step){\n   for(int dir=1;dir<=8;dir*=2){\n    memset(&selection,0,sizeof(selection));memset(gPad,0,sizeof(gPad));\n    entry.flash[0].flags=MFLASH_PAD;entry.flags=ESEL_STARTED;entry.rows=3;\n    entry.colorCount=3;entry.customCount=3;selection.step=step;\n    selection.entry[0].col=2;selection.entry[0].row=1;selection.entry[0].form=2;\n    selection.entry[0].custom=1;selection.entry[0].costume=1;\n    progress.t.tour=tour;gPad[0].gameRepeat=dir;moves=0;int result=-1;\n    int expectedMove=(step==ESEL_STEP_CHARA)||(step==ESEL_STEP_FORM&&(dir==1||dir==2));\n    if(step==ESEL_STEP_PANEL||step==ESEL_STEP_HELP)continue;\n    EntrySel_Input(&result);\n    assert(moves==((tour!=1&&expectedMove)?1:0));\n    if(tour==1){assert(selection.entry[0].custom==1&&selection.entry[0].costume==1);}\n    if(tour!=1&&step==ESEL_STEP_CUSTOM&&(dir==4||dir==8))assert(selection.entry[0].custom!=1);\n    if(tour!=1&&step==ESEL_STEP_COLOR&&(dir==1||dir==2))assert(selection.entry[0].costume!=1);\n   }\n  }\n }\n puts("Tournament input: actual production input handler, five tournaments and cursor directions PASS");\n}\n'
if __name__=='__main__':unittest.main()
