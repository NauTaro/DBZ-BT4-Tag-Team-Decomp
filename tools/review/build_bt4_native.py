import os,pathlib,subprocess,sys,json
if os.name=='nt' and not sys.flags.utf8_mode:
    os.environ['PYTHONUTF8']='1'
    os.execv(sys.executable,[sys.executable,'-X','utf8',*sys.argv])
ws=pathlib.Path.cwd();current=json.loads((ws/'work/PROJECT_CURRENT.json').read_text()) if (ws/'work/PROJECT_CURRENT.json').exists() else {}
root=pathlib.Path(os.environ.get('BT4_SOURCE_ROOT',current.get('source_bt4',str(ws/'work/BT4-native')))).resolve();tools=ws/'work/build-tools/ucrt64/bin'
os.environ['PATH']=str(tools)+os.pathsep+os.environ['PATH'];os.environ['BT3_CC']='win64';os.environ['BT3_SDL3']=str(root/'port/build/win/SDL3-3.4.12/x86_64-w64-mingw32');os.environ['BT3_LIBCLANG']=str(tools/'libclang.dll')
os.chdir(root);sys.path.insert(0,str(root/'port/tools'));import undefined as build
files=['battle/btl_obj.c','battle/view_a_d.c','battle/btl_char_status_anim.c','battle/view_b_c.c','battle/eft_a.c','battle/btl_char_hit.c','battle/btl_char_flag_clash.c','battle/btl_act_e.c','battle/btl_act_f.c','menu/menu_a.c','sys/file.c','sys/movie.c','battle/battle_load.c','battle/btl_script.c','battle/btl_script_cmd.c','battle/hud_e.c','battle/stg_b.c']
if len(sys.argv)>1:files=sys.argv[1:]
if os.environ.get('BT4_HOST_ONLY'):files=[]
log=(ws/'work/build-bt4-native.log').open('w',encoding='utf8')
for name in os.environ.get('BT4_HOST_CPP','').split(','):
    if not name:continue
    f=root/'port/src/gs'/(name+'.cpp');o=build.OBJ/('cxx_'+name+'.o')
    print('Compile C++',name,flush=True)
    r=build.run(['g++','-m32','-std=c++17','-c','-O2','-g','-fno-pic','-msse2','-mfpmath=sse','-fno-exceptions','-fno-rtti','-I'+str(root/'port/third_party/imgui'),'-I'+str(root/'port/src/gs'),'-I'+str(root/'port/build/gen/gs'),str(f),'-o',str(o)],cwd=root,capture_output=True,text=True)
    log.write(r.stderr+'\n');log.flush()
    if r.returncode:print(r.stderr[-4000:]);sys.exit(r.returncode)
for file in files:
    print('Compile',file,flush=True)
    try:obj,src,err,pending=build.cc(root/'src'/file)
    except SystemExit as exc:
        log.write('Compiler aborted for '+file+'\n');log.flush()
        raise SystemExit(exc.code if isinstance(exc.code,int) and exc.code else 1)
    log.write(file+'\n'+err+'\n');log.flush()
    if obj is None:print(err[-3000:],flush=True);sys.exit(1)
for name in os.environ.get('BT4_HOST_FILES','plat_mem,plat_file,plat_verify').split(','):
    if os.environ.get('BT4_SKIP_HOST'):break
    f=root/'port/src'/(name+'.c');o=build.OBJ/('pc_'+name.replace('/','_')+'.o')
    cmd=['gcc','-m32','-std=gnu99','-c','-O2','-g','-fno-pic','-malign-double','-fno-strict-aliasing','-ffp-contract=off','-fno-builtin','-w','-Iinclude','-Iport/include','-Iport/src','-msoft-float','-mno-sse','-mno-mmx','-DSF_FAST_CALLS','-include','math.h','-include','stdlib.h','-include','port_libm.h']
    if name.startswith('gs/'):cmd += ['-I'+str(root/'port/build/win/SDL3-3.4.12/x86_64-w64-mingw32/include')]
    print('Compile',name,flush=True)
    if name.startswith('gs/'):
        o=build.OBJ/('gs_'+f.stem+'.o')
        r=build.run(['gcc','-m32','-std=gnu99','-c','-O2','-g','-fno-pic','-msse2','-mfpmath=sse','-fno-strict-aliasing','-Iport/build/gen/gs',str(f),'-o',str(o)],cwd=root,capture_output=True,text=True)
    else:r=build.compile_ee(cmd,f,o)
    log.write(name+'\n'+r.stderr+'\n');log.flush()
    if r.returncode:print(r.stderr[-3000:],flush=True);sys.exit(1)
print('Link',flush=True);r=subprocess.run([sys.executable,'port/tools/link.py'],stdout=log,stderr=subprocess.STDOUT);log.flush();print('Link result',r.returncode,flush=True);sys.exit(r.returncode)
