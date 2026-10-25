"""Build the native Windows core from this checkout."""
from pathlib import Path
import os,subprocess,sys
root=Path(os.environ.get('BT4_SOURCE_ROOT',Path(__file__).resolve().parents[2])).resolve()
env=dict(os.environ,BT3_CC='win64',BT3_SKELETON='1',PYTHONUTF8='1')
if env.get('BT4_TOOLCHAIN'):
    tool=Path(env['BT4_TOOLCHAIN']);env['PATH']=str(tool)+os.pathsep+env['PATH'];env.setdefault('BT3_LIBCLANG',str(tool/'libclang.dll'))
subprocess.run([sys.executable,'-X','utf8','port/tools/undefined.py'],cwd=root,env=env,check=True)
subprocess.run([sys.executable,'-X','utf8','port/tools/link.py'],cwd=root,env=env,check=True)
