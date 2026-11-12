"""Download the pinned D3D11 implementation, with private dependency paths and licenses."""
import hashlib,json,pathlib,subprocess,urllib.request
ROOT=pathlib.Path(__file__).resolve().parents[2]
def prepare_angle(put,cache=None):
    lock=json.loads((ROOT/'port/release/angle-runtime.lock.json').read_text(encoding='utf-8'))
    cache=pathlib.Path(cache) if cache else ROOT/'port/build/angle-cache';cache.mkdir(exist_ok=True,parents=True)
    digest=lambda b:hashlib.sha256(b).hexdigest()
    for package in lock['packages']:
        archive=cache/package['url'].rsplit('/',1)[1]
        if not archive.exists() or digest(archive.read_bytes())!=package['sha256']:
            temporary=archive.with_suffix(archive.suffix+'.part')
            req=urllib.request.Request(package['url'],headers={'User-Agent':'BT4decomp-runtime'})
            with urllib.request.urlopen(req,timeout=90) as response,temporary.open('wb') as output:
                while block:=response.read(1024*1024):output.write(block)
            if digest(temporary.read_bytes())!=package['sha256']:
                temporary.unlink(missing_ok=True);raise SystemExit('ANGLE runtime archive checksum mismatch')
            temporary.replace(archive)
        for member,item in package['files'].items():
            # Read only explicitly pinned members. No archive paths are extracted.
            result=subprocess.run(['tar','-xOf',str(archive),member],capture_output=True)
            if result.returncode or digest(result.stdout)!=item['sha256']:raise SystemExit('ANGLE runtime member checksum mismatch: '+member)
            put(item['target'],result.stdout)
    put('licenses/angle/SOURCES.json',json.dumps(lock,indent=2).encode())
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--target',type=pathlib.Path,required=True);parser.add_argument('--cache',type=pathlib.Path)
    args=parser.parse_args()
    def write(relative,data):
        output=args.target/relative;output.parent.mkdir(exist_ok=True,parents=True);output.write_bytes(data)
    prepare_angle(write,args.cache);print('Verified DirectX 11 runtime and license files prepared')
