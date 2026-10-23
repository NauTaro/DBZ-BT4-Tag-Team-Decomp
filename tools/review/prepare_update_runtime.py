"""Prepare BT4 test runtime without modifying a live installation or extracted data."""
from pathlib import Path
import os,sys,zipfile,shutil,hashlib,struct,json
from audit_mod import flat
source=Path(sys.argv[1]).resolve();target=Path(sys.argv[2]).resolve()
data=Path(os.environ.get('BT4_DATA_ROOT',str(Path.home()/'Desktop/DBZ BT4 LATINO PORTABLE/gamedata')))
target.mkdir(parents=True,exist_ok=True)
assets=Path.home()/'Desktop/DBZ BT4 LATINO PORTABLE'
for d in ['fonts','translations','licenses','tbo','textures','songs','stages']:
    shutil.copytree(assets/d,target/d,dirs_exist_ok=True)
manifest=source/'port/runtime-assets.json'
if manifest.exists():
    for item in json.loads(manifest.read_text(encoding='utf8')):
        src=(source/item['source']).resolve();dst=(target/item['target']).resolve()
        assert src.is_relative_to(source) and dst.is_relative_to(target)
        assert Path(item['target']).parts[0] in ('tbo','fonts','translations','licenses') or item['target'] in ('libdatachannel.dll','libopus-0.dll','libwinpthread-1.dll','BT4_VERSION.txt')
        assert hashlib.sha256(src.read_bytes()).hexdigest()==item['sha256']
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
for n in ['LICENSE','LICENSE-MIT']:shutil.copy2(source/n,target/n)
report_path=source/'UPDATE_REPORT.json'
upstream_parent=Path(json.loads(report_path.read_text())['upstream']).parent if report_path.exists() else None
lock_default=next((p for p in ([upstream_parent/'LOCK.json',upstream_parent/'UPSTREAM_LOCK.json'] if upstream_parent else []) if p.exists()),None)
lock_path=Path(sys.argv[3] if len(sys.argv)>3 else os.environ.get('BT4_UPSTREAM_LOCK',str(lock_default or 'UPSTREAM_LOCK.json')))
lock=json.loads(lock_path.read_text());asset=next(x for x in lock['assets'] if x['name']=='Tenkaichi3Decomp-windows-x64.zip')
archive=Path(os.environ.get('BT4_UPSTREAM_ARCHIVE',asset.get('archive',str(lock_path.parent/asset['name']))))
assert hashlib.sha256(archive.read_bytes()).hexdigest()==asset['sha256']
with zipfile.ZipFile(archive) as z:
    for n in z.namelist():
        rel=Path(*Path(n).parts[1:])
        if n.endswith('/') or rel.name in ['Tenkaichi3Decomp.exe','Tenkaichi3Decomp.dat','Tenkaichi3Decomp.exe.mem','Tenkaichi3Decomp-setup.exe','README.txt']:continue
        p=target/rel;assert p.resolve().is_relative_to(target)
        p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(z.read(n))
build=source/'port/build'
shutil.copy2(source/'port/third_party/fonts/fonts.tsv',target/'fonts/fonts.tsv')
for a,b in [('bt3.exe','Tenkaichi3Decomp.exe'),('bt3.exe.mem','Tenkaichi3Decomp.exe.mem')]:shutil.copy2(build/a,target/b)
blob=bytearray((build/'bt3.dat').read_bytes());count=struct.unpack_from('<I',blob,4)[0]
programs=[flat((data/'disc/SLUS_219.78').read_bytes()),(data/'disc/BIN/DBZP.BIN').read_bytes()]
assert [hashlib.sha1(b).hexdigest() for b in programs]==['2ae9c636de479c01d2b4e9906c25b7ca807e80d9','de91913f40bd675d70ee748aa16a42a96dff5919']
h=0xcbf29ce484222325
for i in range(count):
    _,which,off,size=struct.unpack_from('<4I',blob,16+i*16)
    assert which<2 and off+size<=len(programs[which])
    for b in programs[which][off:off+size]:h=((h^b)*0x100000001b3)&0xffffffffffffffff
struct.pack_into('<Q',blob,8,h);(target/'Tenkaichi3Decomp.dat').write_bytes(blob)
(target/'bt3_settings.txt').write_text('fullscreen=0\nscale=2\naspect_milli=1778\nhud_layout=0\nmenu_hd=1\nmenu_hd_scale=4\ngame_speed=100\ntext_font_id=600621650\nskip_logos=1\nskip_intro=1\nskip_copyright=1\nlanguage_prompt_always=0\n',encoding='utf8')
(target/'port/build/shots').mkdir(parents=True,exist_ok=True)
print('Prepared:',target)
