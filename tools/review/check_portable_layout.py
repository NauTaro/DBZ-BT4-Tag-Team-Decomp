"""Validate portable installation/release plans without deleting user files."""
from pathlib import Path
import argparse
import json

POLICY_PATH = Path(__file__).resolve().parents[2] / 'port/release/PORTABLE_LAYOUT.json'

def check(root):
    root = Path(root)
    policy = json.loads(POLICY_PATH.read_text(encoding='utf8'))
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
    try:
        result = check(args.root)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(json.dumps(dict(root=str(args.root.resolve()), passed=False,
                              error=str(exc), files_deleted=[]), indent=2))
        raise SystemExit(2)
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result['passed'] else 1)
