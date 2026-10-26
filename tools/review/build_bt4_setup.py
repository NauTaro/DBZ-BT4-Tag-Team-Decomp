"""Build the current installer after the core's ImGui objects exist."""
from pathlib import Path
import os,subprocess
root=Path(os.environ.get('BT4_SOURCE_ROOT',Path(__file__).resolve().parents[2])).resolve();env=dict(os.environ)
if env.get('BT4_TOOLCHAIN'):env['PATH']=env['BT4_TOOLCHAIN']+os.pathsep+env['PATH']
out=root/'port/build/setup_bt4';out.mkdir(parents=True,exist_ok=True)
inc=root/'port/third_party/imgui';sdl=Path(env.get('BT3_SDL3',root/'port/build/win/SDL3-3.4.12/x86_64-w64-mingw32'))
def run(args):subprocess.run(args,cwd=root,env=env,check=True)
for name in ['native','setup']:
    run(['g++','-std=c++17','-O2','-I'+str(inc),'-I'+str(sdl/'include'),'-c',str(root/'port/setup'/(name+'.cpp')),'-o',str(out/(name+'.o'))])
objects=[str(p) for p in (root/'port/build/obj_win64').glob('cxx_imgui*.o') if p.stem!='cxx_imgui_impl_dx9'];assert objects,'Build the native core first'
run(['g++','-o',str(out/'BT4-Setup.exe')]+objects+[str(out/'native.o'),str(out/'setup.o'),'-L'+str(sdl/'lib'),'-Wl,-Bdynamic','-lSDL3','-ladvapi32','-Wl,-Bstatic','-lz','-lstdc++','-lwinpthread','-static-libgcc','-mwindows'])
(out/'entry.rc').write_text('1 ICON "'+(root/'port/release/icon_game.ico').as_posix()+'"\n')
run(['windres',str(out/'entry.rc'),'-O','coff','-o',str(out/'entry.o')])
run(['gcc',str(root/'tools/review/setup_entry.c'),str(out/'entry.o'),'-municode','-mwindows','-Os','-s','-lshell32','-o',str(out/'Install.exe')])
