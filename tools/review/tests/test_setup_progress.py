"""Exercise the installer's progress contract without game data or SDL."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest


class SetupProgressTests(unittest.TestCase):
    def test_progress_contract(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        root = pathlib.Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory() as directory:
            temp = pathlib.Path(directory)
            source = temp / "progress.cpp"
            source.write_text(r'''
#include "port/setup/progress.h"
#include <cassert>
int main(){
    SetupProgress p;
    assert(p.read("2147483648 4294967296 10 20"));
    assert(p.fraction==0.5 && p.bytes==2147483648ULL);
    assert(!p.read("5 4 1 2"));
    assert(!p.read("1 2 3 2"));
    assert(!p.read("1 0 0 1"));
    assert(!p.read("-1 100 1 2"));
    assert(!p.read("1 2 1 2 junk"));
    assert(p.fraction==0.5);
    assert(p.read("1 4294967296 1 20") && p.fraction==0.5);
    assert(SetupPercent(100,100,false)<100);
    assert(SetupPercent(100,100,true)==100);
    assert(SetupPercent(0,0,false)==0);
    assert(SetupStepWeight("Game data")>SetupStepWeight("Disc image"));
}
''', encoding="utf-8")
            exe = temp / "progress.exe"
            subprocess.run([compiler, "-std=c++17", "-I", str(root), str(source), "-o", str(exe)], check=True, capture_output=True)
            subprocess.run([str(exe)], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
