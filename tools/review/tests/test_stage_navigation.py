"""Exercise the game's C navigation routines with native and installed maps."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]

class StageNavigationTests(unittest.TestCase):
    def test_grid_padding_and_installed_maps(self):
        compiler = shutil.which('cc') or shutil.which('gcc')
        if not compiler:
            self.skipTest('A C compiler is needed for the native navigation regression')
        text = (ROOT / 'src/battle/view_a_e.c').read_text(encoding='utf8')
        routines = text[text.index('static s32 StgGrid_IsStage'):text.index('#define CHARA_UNLOCKED')]
        duel = (ROOT / 'src/menu/menu_c_e.c').read_text(encoding='utf8')
        team = (ROOT / 'src/menu/menu_e_b.c').read_text(encoding='utf8')
        empty = lambda t: re.search(r'^#define STGGRID_ID_EMPTY\s+(0x[0-9a-fA-F]+)', t, re.M).group(1)
        code = '''#include <assert.h>
#include <stdio.h>
typedef int s32;
#define PORT 1
#define STGGRID_COLS 6
#define PORT_STAGE_FIRST 99
int gPortExtraStageCount, gPortExtraStages[26];
''' + '#define DUEL_EMPTY ' + empty(duel) + '\n#define TEAM_EMPTY ' + empty(team) + '\n' + routines + r'''
int main(void) {
    int storage[134], *ids = storage + 1, i, n, row, col, step;
    unsigned rng = 1;
    storage[0] = 0x12345; storage[133] = 0x54321;
    assert(!StgGrid_IsStage(DUEL_EMPTY));
    assert(!StgGrid_IsStage(TEAM_EMPTY));
    assert(!StgGrid_IsStage(-1));
    assert(!StgGrid_IsStage(99));
    assert(!StgGrid_IsStage(125));
    for (n = 1; n <= 26; ++n) {
        gPortExtraStageCount = n;
        for (i = 0; i < 26; ++i) gPortExtraStages[i] = 99 + i;
        for (i = 0; i < 132; ++i) ids[i] = DUEL_EMPTY;
        for (i = 0; i < 66; ++i) ids[i] = i < 36 ? i : i + 3;
        for (i = 0; i < n; ++i) ids[66 + i] = 99 + i;
        assert(StgGrid_IsSelectable(ids, 66));
        if (n == 1) {
            row = 11; col = 0;
            StgGrid_MoveRight(ids, &col, row); assert(col == 6);
            StgGrid_MoveLeft(ids, &col, row); assert(col == 0);
            assert(!StgGrid_MoveRightWrap(ids, &col, row)); assert(col == 0);
            row = 0; col = 5;
            StgGrid_MoveUp(ids, &col, &row, 12); assert(row == 11 && col == 0);
        }
        row = 0; col = 0;
        for (step = 0; step < 6000; ++step) {
            rng = rng * 1664525u + 1013904223u;
            switch ((rng >> 16) & 3) {
                case 0: StgGrid_MoveRight(ids, &col, row); break;
                case 1: StgGrid_MoveLeft(ids, &col, row); break;
                case 2: if (col != 6) StgGrid_MoveDown(ids, &col, &row, (66+n+5)/6); break;
                case 3: if (col != 6) StgGrid_MoveUp(ids, &col, &row, (66+n+5)/6); break;
            }
            assert(row >= 0 && row < (66+n+5)/6 && col >= 0 && col <= 6);
            if (col != 6) assert(StgGrid_IsSelectable(ids, row*6+col));
            assert(storage[0] == 0x12345 && storage[133] == 0x54321);
        }
    }
    puts("156000 moves passed; native padding skipped; 26 installed-map counts checked");
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'navigation.c'
            executable = root / ('navigation.exe' if os.name == 'nt' else 'navigation')
            source.write_text(code, encoding='utf8')
            flags = {'creationflags': 0x08000000} if os.name == 'nt' else {}
            compiled = subprocess.run([compiler, '-std=c99', '-O2', str(source), '-o', str(executable)],
                capture_output=True, text=True, timeout=45, **flags)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            checked = subprocess.run([str(executable)], capture_output=True, text=True, timeout=20, **flags)
            self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)
            self.assertIn('156000 moves passed', checked.stdout)

if __name__ == '__main__':
    unittest.main()
