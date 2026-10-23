"""Validate portable installation/release plans without deleting user files."""
from pathlib import Path
import argparse
import json

policy_path = Path(__file__).resolve().parent / 'fixes-r16/BT4-TAG/port/release/PORTABLE_LAYOUT.json'

def check(root):
    policy = json.loads(policy_path.read_text(encoding='utf8'))
    allowed_dirs = {name.casefold() for name in policy['root_directories']}
    allowed_files = {name.casefold() for name in policy['root_files']}
    unexpected = [p.name for p in root.iterdir()
                  if p.name.casefold() not in (allowed_dirs if p.is_dir() else allowed_files)]
    return dict(root=str(root.resolve()), passed=not unexpected,
                unexpected_root_entries=sorted(unexpected), files_deleted=[])

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('root', type=Path)
    args = parser.parse_args()
    result = check(args.root)
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result['passed'] else 1)
