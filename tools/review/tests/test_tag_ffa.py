from pathlib import Path
import os,shutil,subprocess,sys,tempfile,unittest

ROOT=Path(__file__).resolve().parents[3]

class FreeForAllTests(unittest.TestCase):
    def test_six_player_admission_draft_and_wire_validation(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        flags={'creationflags':0x08000000} if os.name=='nt' else {}
        with tempfile.TemporaryDirectory() as directory:
            exe=Path(directory)/('room.exe' if os.name=='nt' else 'room')
            p=subprocess.run([cc,'-std=c99','-I'+str(ROOT/'port/include'),
                str(ROOT/'port/tests/tag_timing/test_ffa_room.c'),str(ROOT/'port/src/tag_team.c'),
                str(ROOT/'port/src/tag_team_wire.c'),'-o',str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stderr)
            p=subprocess.run([str(exe)],capture_output=True,text=True,**flags)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)

    def test_battle_results_identify_every_survivor_and_preserve_duel(self):
        cc=shutil.which('cc') or shutil.which('gcc')
        if not cc:self.skipTest('A C compiler is required')
        env=dict(os.environ,CC=cc)
        p=subprocess.run([sys.executable,str(ROOT/'port/tests/tag_timing/test_ffa_result.py')],
                         env=env,capture_output=True,text=True,timeout=45)
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)

if __name__=='__main__':unittest.main()
