#!/usr/bin/env python3
"""Restore the pinned Windows voice SDK and GitHub authorization helper.

The bootstrap archive is verified before extracting the explicit dependency set.
No game executable, installer, settings, saves, ISO or extracted game data is used.
"""
import argparse, hashlib, json, pathlib, urllib.request, zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
URL = "https://github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases/download/bt4-r26-rc1/BT4-Tag-Team-R26-RC1.zip"
SHA256 = "935a14f57273ffef6667ff7ab4070503a6bc41987125275a4e05b4da6177d167"
REQUIRED = {
    "libdatachannel.dll", "libopus-0.dll", "libwinpthread-1.dll",
    "tbo/setup/gh.exe", "licenses/GitHub-CLI-MIT.txt",
    "licenses/source/VOICE-SDK-source.zip", "licenses/voice/SOURCES.json",
}

def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        while block := stream.read(1024*1024):
            digest.update(block)
    return digest.hexdigest()

def prepare(archive=None):
    archive = pathlib.Path(archive).resolve() if archive else ROOT / 'port/build/runtime-base.zip'
    if not archive.exists():
        archive.parent.mkdir(parents=True, exist_ok=True)
        temporary = archive.with_suffix('.part')
        with urllib.request.urlopen(URL, timeout=120) as response, temporary.open('wb') as out:
            while block := response.read(1024*1024):
                out.write(block)
        if sha(temporary) != SHA256:
            temporary.unlink()
            raise SystemExit('Windows runtime bootstrap checksum mismatch')
        temporary.replace(archive)
    if sha(archive) != SHA256:
        raise SystemExit('Windows runtime bootstrap checksum mismatch')
    target = ROOT / 'port/build/windows-runtime'
    manifest = {}
    with zipfile.ZipFile(archive) as zipped:
        names = set(zipped.namelist())
        if not REQUIRED <= names:
            raise SystemExit('Windows runtime bootstrap is incomplete')
        selected = REQUIRED | {n for n in names if n.startswith('licenses/voice/') and not n.endswith('/')}
        for name in sorted(selected):
            parts = pathlib.PurePosixPath(name).parts
            if '..' in parts or pathlib.PurePosixPath(name).is_absolute():
                raise SystemExit('Invalid Windows runtime dependency path')
            data = zipped.read(name)
            output = target.joinpath(*parts)
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(data)
            manifest[name] = hashlib.sha256(data).hexdigest()
    (target/'manifest.json').write_text(json.dumps({'bootstrap_url':URL,'bootstrap_sha256':SHA256,'files':manifest},indent=2)+'\n')
    print(f'Prepared {len(manifest)} verified Windows runtime dependencies')

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=pathlib.Path)
    prepare(parser.parse_args().archive)
