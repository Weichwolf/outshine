#!/usr/bin/env python3
"""Format or check the repository-owned C++ population; never walk vendor environments."""
import argparse
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BATCH_FILES = 64


def sources(root):
    listed = subprocess.check_output(
        ['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard',
         '--', 'src', 'include', 'test'], cwd=root)
    paths = {Path(os.fsdecode(name)) for name in listed.split(b'\0') if name}
    return sorted(path for path in paths
                  if path.suffix in ('.cpp', '.h', '.mm')
                  and path.parts[:3] != ('src', 'render', 'shaders')
                  and (root / path).is_file())


def format_sources(root, tool, check):
    try:
        paths = sources(root)
        if not paths:
            raise ValueError('no repository-owned C++ files; formatting coverage is empty')
        flags = ['--dry-run', '--Werror'] if check else ['-i']

        def run_batch(batch):
            result = subprocess.run([tool, *flags, *batch], cwd=root, timeout=60, check=False,
                                    capture_output=True, text=True)
            if result.returncode and len(batch) > 1:
                return sum(run_batch([path]) for path in batch)
            sys.stdout.write(result.stdout)
            sys.stderr.write(result.stderr)
            return int(result.returncode != 0)

        failed = sum(run_batch(paths[start:start + BATCH_FILES])
                     for start in range(0, len(paths), BATCH_FILES))
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
