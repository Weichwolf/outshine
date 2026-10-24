#!/usr/bin/env python3
"""Format or check the repository-owned C++ population; never walk vendor environments."""
import argparse
import json
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BATCH_FILES = 64


def sources(root):
    listed = subprocess.check_output(
        ['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard',
         '--', 'src', 'test'], cwd=root)
    paths = {Path(os.fsdecode(name)) for name in listed.split(b'\0') if name}
    return sorted(path for path in paths
                  if path.suffix in ('.cpp', '.h', '.hpp', '.mm')
                  and path.parts[:3] != ('src', 'render', 'shaders')
                  and (root / path).is_file())


def format_sources(root, tool, check, selected=None):
    try:
        paths = sources(root)
        if not paths:
            raise ValueError('no repository-owned C++ files; formatting coverage is empty')
        if selected is not None:
            selected = {Path(path) for path in selected}
            if selected - set(paths):
                raise ValueError('format selection contains files outside the owned population')
            paths = [path for path in paths if path in selected]
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
    parser.add_argument('--scope', type=Path)
    args = parser.parse_args()
    selected = json.loads(args.scope.read_text())['format'] if args.scope else None
    return format_sources(ROOT, args.tool, args.check, selected)


if __name__ == '__main__':
    sys.exit(main())
