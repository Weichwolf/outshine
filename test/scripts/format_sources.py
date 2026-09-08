#!/usr/bin/env python3
"""Format or check the repository-owned C++ population; never walk vendor environments."""
import argparse
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def sources(root):
    listed = subprocess.check_output(
        ['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard',
         '--', 'src', 'include', 'test'], cwd=root)
    paths = {Path(os.fsdecode(name)) for name in listed.split(b'\0') if name}
    return sorted(path for path in paths
                  if path.suffix in ('.cpp', '.h') and 'shaders' not in path.parts
                  and (root / path).is_file())


def format_sources(root, tool, check):
    try:
        paths = sources(root)
        if not paths:
            raise ValueError('no repository-owned C++ files; formatting coverage is empty')
        flags = ['--dry-run', '--Werror'] if check else ['-i']
        failed = 0
        for path in paths:
            result = subprocess.run([tool, *flags, path], cwd=root, timeout=60, check=False,
                                    capture_output=True, text=True)
            sys.stdout.write(result.stdout)
            sys.stderr.write(result.stderr)
            failed += result.returncode != 0
        print(f'format: {len(paths)} owned C++ files {"checked" if check else "formatted"}; '
              f'{failed} failed')
        return 1 if failed else 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'format: {error}', file=sys.stderr)
        return 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', required=True)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    return format_sources(ROOT, args.tool, args.check)


if __name__ == '__main__':
    sys.exit(main())
