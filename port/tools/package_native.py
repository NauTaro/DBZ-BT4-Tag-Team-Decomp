"""Native release folders with bundled SDL, TLS transport and Opus libraries."""
import json,os,pathlib,re,shutil,subprocess,sys,zipfile

def run(*args):
    return subprocess.check_output(list(map(str,args)),text=True).strip()

def libraries_linux(out):
    search=['/usr/local/lib','/usr/lib/x86_64-linux-gnu','/usr/lib64','/usr/lib']
    def find(name):
        return next((pathlib.Path(d)/name for d in search if (pathlib.Path(d)/name).exists()),None)
    seeds=[find(name) for name in ('libSDL3.so.0','libcurl.so.4','libopus.so.0')]
    if any(p is None for p in seeds):raise RuntimeError('SDL3, WebSocket-enabled libcurl and Opus are required')
    queue=[out/'BT4decomp',out/'BT4decomp-setup',*seeds];done=set();sources={}
    core=re.compile(r'^(?:ld-linux|lib(?:c|m|pthread|dl|rt|resolv|gcc_s|stdc\+\+)\.so)')
    while queue:
        file=queue.pop(0)
        if str(file) in done:continue
        done.add(str(file))
        deps=[]
        for line in run('ldd',file).splitlines():
            match=re.match(r'\s*(\S+) => (/\S+)',line)
            if 'not found' in line:raise RuntimeError(str(file)+': '+line)
            if match and not core.match(match[1]):deps.append((match[1],pathlib.Path(match[2])))
        if file in seeds:deps.append((file.name,file))
        for name,source in deps:
            if name in sources:continue
            sources[name]=str(source.resolve());shutil.copy2(source.resolve(),out/'lib'/name);queue.append(source)
    for file in (out/'lib').iterdir():
        if file.suffix!='.pem':subprocess.run(['patchelf','--set-rpath','$ORIGIN',str(file)],check=True)
    return sources

def libraries_mac(out):
    seeds=[]
    for env,default,name in [('BT3_SDL3','/opt/homebrew/opt/sdl3','libSDL3.0.dylib'),
                              ('BT3_CURL','/opt/homebrew/opt/curl','libcurl.4.dylib'),
                              ('BT3_OPUS','/opt/homebrew/opt/opus','libopus.0.dylib')]:
        source=pathlib.Path(os.environ.get(env,default))/'lib'/name
        if not source.exists():raise RuntimeError('Missing native library: '+str(source))
        seeds.append(source)
    queue=[out/'BT4decomp',out/'BT4decomp-setup',*seeds];done=set();sources={};changes={}
    while queue:
        file=queue.pop(0);key=str(file)
        if key in done:continue
        done.add(key);deps=[]
        for line in run('otool','-L',file).splitlines()[1:]:
            name=line.strip().split(' (',1)[0]
            if name.startswith(('/opt/homebrew/','/usr/local/')):deps.append(pathlib.Path(name))
        if file in seeds:deps.append(file)
        changes[key]=deps
        for source in deps:
            if source.name in sources:continue
            sources[source.name]=str(source.resolve());shutil.copy2(source.resolve(),out/'lib'/source.name);queue.append(source)
    for original,deps in changes.items():
        original=pathlib.Path(original);dest=original if original.parent==out else out/'lib'/original.name
        if not dest.exists():continue
        for dep in deps:
            if dep==original:continue
            relative=('@executable_path/lib/' if dest.parent==out else '@loader_path/')+dep.name
            subprocess.run(['install_name_tool','-change',str(dep),relative,str(dest)],check=True)
        if dest.parent!=out:subprocess.run(['install_name_tool','-id','@rpath/'+dest.name,str(dest)],check=True)
    for file in [*sorted((out/'lib').glob('*.dylib')),out/'BT4decomp',out/'BT4decomp-setup']:
        subprocess.run(['codesign','--force','--sign','-',str(file)],check=True)
    return sources

def main(p,mac=False):
    root=p.ROOT;out=root/'port/build'/('release-mac' if mac else 'release')
    archive=root/'port/build'/('BT4decomp-macos-arm64.zip' if mac else 'BT4decomp-linux-x64.zip')
    exe=root/'port/build'/('bt3_mac' if mac else 'bt3_64');setup=root/'BT4decomp-setup'
    if not exe.is_file() or not setup.is_file():raise RuntimeError('Build the native runtime and setup first')
    if out.exists():
        assert out.resolve().is_relative_to((root/'port/build').resolve()) and not out.is_symlink()
        shutil.rmtree(out)
    p.take_program(exe,out);p.take_lang(out);p.take_translations(out)
    shutil.copy2(setup,out/'BT4decomp-setup');shutil.copy2(root/'port/release/BT4_VERSION.txt',out/'BT4_VERSION.txt')
    for name in ('lib','licenses','textures','stages','songs'):(out/name).mkdir()
    licenses=out/'licenses'
    for source,target in [('LICENSE','bt4decomp-agpl.txt'),('LICENSE-MIT','bt4decomp-mit.txt'),
      ('port/third_party/imgui/LICENSE.txt','dear-imgui.txt'),('port/third_party/newlib_libm/COPYING.NEWLIB','newlib.txt'),
      ('port/third_party/xxhash/LICENSE','xxhash.txt'),('port/third_party/miniaudio/LICENSE','miniaudio.txt'),
      ('port/third_party/miniz/LICENSE','miniz.txt'),('port/third_party/monocypher/LICENCE.md','monocypher.txt'),
      ('port/third_party/voice/LICENSE-opus','opus.txt')]:shutil.copy2(root/source,licenses/target)
    (licenses/'sdl3.txt').write_text('SDL3: zlib license. https://www.libsdl.org/license.php\n')
    (licenses/'curl.txt').write_text('curl license: https://curl.se/docs/copyright.html\n')
    p.take_fonts(out,licenses);p.take_tbo_art(out,licenses)
    for name,note in [('textures',p.TEXTURES_NOTE),('stages',p.STAGES_NOTE),('songs',p.SONGS_NOTE)]:
        (out/name/'README.txt').write_text(note,encoding='utf8')
    readme=p.README.replace('(Linux, 64-bit)','(macOS, Apple Silicon arm64)' if mac else '(Linux, x86-64)')
    readme+='\nNative voice uses the TLS Opus relay. Direct WebRTC voice is available when a compatible libdatachannel is installed.\n'
    if mac:readme+='This macOS package is ad-hoc signed, not notarized. Allow the downloaded application in macOS Privacy & Security if Gatekeeper blocks it. Intel Macs are not supported by this arm64 build.\n'
    (out/'README.txt').write_text(readme,encoding='utf8')
    sources=libraries_mac(out) if mac else libraries_linux(out)
    (licenses/'native-libraries.json').write_text(json.dumps(sources,indent=2))
    # Public CA certificates, never private account material.
    ca=next((pathlib.Path(x) for x in ['/etc/ssl/certs/ca-certificates.crt','/opt/homebrew/etc/ca-certificates/cert.pem','/etc/ssl/cert.pem'] if pathlib.Path(x).is_file()),None)
    if ca:shutil.copy2(ca,out/'lib/cacert.pem')
    for file in [out/'BT4decomp',out/'BT4decomp-setup',*(out/'lib').glob('*')]:
        if file.suffix!='.pem':file.chmod(0o755)
    if not mac:p.portable(out)
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for file in sorted(out.rglob('*')):
            if file.is_file():z.write(file,'BT4decomp/'+file.relative_to(out).as_posix())
    print(str(archive.relative_to(root))+': '+str(archive.stat().st_size)+' bytes')
