import os,pathlib,subprocess,sys,json
current=json.loads(pathlib.Path('work/PROJECT_CURRENT.json').read_text()) if pathlib.Path('work/PROJECT_CURRENT.json').exists() else {}
root=pathlib.Path(os.environ.get('BT4_SOURCE_ROOT',current.get('source_bt4','work/BT4-native'))).resolve();bin=pathlib.Path('work/build-tools/ucrt64/bin').resolve()
env=os.environ.copy();env['PATH']=str(bin)+os.pathsep+env['PATH']
os.environ['PATH']=env['PATH']
out=root/'port/build/setup_bt4';out.mkdir(exist_ok=True)
inc=root/'port/third_party/imgui';sdl=root/'port/build/win/SDL3-3.4.12/x86_64-w64-mingw32'
log=pathlib.Path('work/build-bt4-setup.log').open('w')
for name in ['native','setup']:
    cmd=['g++','-std=c++17','-O2','-w','-I'+str(inc),'-I'+str(sdl/'include'),'-c',str(root/'port/setup'/(name+'.cpp')),'-o',str(out/(name+'.o'))]
    print('Compile setup',name,flush=True)
    r=subprocess.run(cmd,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT);log.flush()
    if r.returncode:sys.exit(r.returncode)
objects=[str(p) for p in (root/'port/build/obj_win64').glob('cxx_imgui*.o')]+[str(out/'native.o'),str(out/'setup.o')]
cmd=['g++','-o',str(root/'port/build/Tenkaichi3Decomp-setup.exe')]+objects+['-L'+str(sdl/'lib'),'-Wl,-Bdynamic','-lSDL3','-Wl,-Bstatic','-lstdc++','-lwinpthread','-static-libgcc','-mwindows']
r=subprocess.run(cmd,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT);log.flush();print('Setup build result',r.returncode,flush=True);sys.exit(r.returncode)
