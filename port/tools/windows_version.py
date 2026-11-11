"""Generate consistent first-party Windows publisher/version resources."""
import argparse
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[2]

def render(role):
    version=(ROOT/'port/include/bt4_version.h').read_text(encoding='utf-8')
    def value(key):
        return re.search(r'^#define '+key+r' "([^"]+)"$',version,re.M)[1]
    numeric=value('BT4_UPSTREAM_VERSION').split('.')
    assert len(numeric)==4 and all(n.isdigit() and int(n)<=65535 for n in numeric)
    canonical=value('BT4_CANONICAL_VERSION')
    filename='BT4decomp.exe' if role=='game' else 'BT4decomp-setup.exe'
    description='BT4decomp' if role=='game' else 'BT4decomp Setup'
    icon=(ROOT/'port/release/game_icon.rc').as_posix()
    return f'''#include "{icon}"
1 VERSIONINFO
 FILEVERSION {','.join(numeric)}
 PRODUCTVERSION {','.join(numeric)}
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS 0x40004L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904b0"
  BEGIN
   VALUE "CompanyName", "NauTaro\\0"
   VALUE "FileDescription", "{description}\\0"
   VALUE "FileVersion", "{canonical}\\0"
   VALUE "InternalName", "{filename}\\0"
   VALUE "OriginalFilename", "{filename}\\0"
   VALUE "ProductName", "BT4decomp\\0"
   VALUE "ProductVersion", "{canonical}\\0"
   VALUE "Comments", "Upstream copyright and license notices are preserved in the distributed source and LICENSE files.\\0"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x0409, 1200
 END
END
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('role',choices=['game','setup'])
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    content=render(args.role)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8')!=content:
        args.output.write_text(content,encoding='utf-8')

if __name__=='__main__':
    main()
