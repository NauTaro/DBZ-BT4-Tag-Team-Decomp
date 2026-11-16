"""Exercise the native stage break hook with match state that can be rewound."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from test_potara import function

ROOT = Path(__file__).resolve().parents[3]


class RankedRockStageTests(unittest.TestCase):
    def test_native_break_only_rewards_the_selected_story_rock(self):
        cc = shutil.which("gcc") or shutil.which("cc")
        if not cc:
            self.skipTest("C compiler required")
        source = (ROOT / "src/battle/stg_a_b.c").read_text(encoding="utf-8")
        hook = "static void " + function(source, "StgObj_FindRankedBall")
        event = "unsigned " + function(source, "Port_RankedRockEvent")
        program = r"""
#include <assert.h>
#include "battle/ranked_rock.h"
#include <stdint.h>
typedef int s32;
#define STG_OBJ_ITEM 0x20000000
typedef struct { unsigned type; } StgObj;
typedef struct { StgObj *objDefs; } StgData;
typedef struct { unsigned objCount; StgObj *objs; StgData *data; } Stg;
typedef struct { s32 unk44; } BattleResult;
static StgObj rocks[] = {{0}, {STG_OBJ_ITEM}, {STG_OBJ_ITEM}, {0}};
static StgData defs = {rocks};
static Stg stage = {4, rocks, &defs}, *gBtlStage = &stage;
static BattleResult result;
static unsigned seed, sounds, marks;
static int battle = 3;
unsigned Port_RankedRockSeed(void) { return seed; }
int Port_BattleSeqState(void) { return battle; }
int BattleSide_GetObjId(int side) { return 20 + side; }
int BtlStage_GetId(void) { return 39; }
BattleResult *BattleResult_GetPtr(void) { return &result; }
void StreamSe_PlayDefault(int a, int b) { assert(a == 0 && b == 0x8d3a); ++sounds; }
void HudTimer_ShowMark(void) { ++marks; }
""" + hook + event + r"""
int main(void) {
    StgObj_FindRankedBall(20, 1);
    assert(!result.unk44 && !sounds);
    seed = 2;
    StgObj_FindRankedBall(20, 0);
    StgObj_FindRankedBall(20, 1);
    StgObj_FindRankedBall(-1, 2);
    StgObj_FindRankedBall(22, 2);
    assert(!result.unk44 && !sounds);
    battle = 2;
    StgObj_FindRankedBall(21, 2);
    assert(!result.unk44);
    battle = 3;
    StgObj_FindRankedBall(21, 2);
    assert(Port_RankedRockEvent() == RankedRock_Event(39, 2, 1));
    StgObj_FindRankedBall(20, 2);
    assert(Port_RankedRockEvent() == RankedRock_Event(39, 2, 1));
    assert(marks == 1 && sounds == 1);
    result.unk44 = 0; // snapshot before the hit
    StgObj_FindRankedBall(20, 2);
    assert(Port_RankedRockEvent() == RankedRock_Event(39, 2, 0));
    seed = 0;
    assert(!Port_RankedRockEvent());
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            src = path / "rock.c"
            exe = path / ("rock.exe" if os.name == "nt" else "rock")
            src.write_text(program, encoding="utf-8")
            options = {"creationflags": 0x08000000} if os.name == "nt" else {}
            build = subprocess.run(
                (["rtk", "proxy"] if shutil.which("rtk") else []) + [cc, "-I" + str(ROOT / "include"), str(src), "-o", str(exe)],
                capture_output=True, text=True, **options
            )
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run((["rtk", "proxy"] if shutil.which("rtk") else []) + [str(exe)], capture_output=True, text=True, timeout=15, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
