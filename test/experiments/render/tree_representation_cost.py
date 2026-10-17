"""Compare native tree expansion and instancing; CPU payloads, not GPU timings."""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
PACKAGES = ('sdl3', 'sdl3-image', 'sdl3-ttf', 'liblz4',
            'libtiff-4', 'libwebp', 'geos', 'sqlite3', 'libzstd')


def compiler_command():
    records = json.loads((ROOT / 'compile_commands.json').read_text())
    record = next(row for row in records
                  if Path(row['file']).resolve() == ROOT / 'src/generators/flora/TreePrototype.cpp')
    flags = record.get('arguments') or shlex.split(record['command'])
    command = []
    skip = False
    for flag in flags:
        if skip:
            skip = False
        elif flag == '-o':
            skip = True
        elif flag != '-c' and Path(flag) != Path(record['file']):
            command.append(flag)
    return command


def checked(command, **kwargs):
    with tempfile.TemporaryFile(mode='w+', encoding='utf-8') as log:
        result = subprocess.run(command, cwd=ROOT, stderr=log, timeout=180, check=False, **kwargs)
        if result.returncode:
            log.seek(0)
            raise RuntimeError(f'exit {result.returncode}: {shlex.join(command)}\n{log.read()[-6000:]}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('species', nargs='*', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    species = args.species or [ROOT / f'src/assets/world/species/{name}.json'
                              for name in ('birch', 'oak', 'spruce')]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    package_path = ROOT / 'build/deps/install/lib/pkgconfig'
    environment['PKG_CONFIG_PATH'] = str(package_path) + os.pathsep + environment.get('PKG_CONFIG_PATH', '')
    libraries = shlex.split(subprocess.check_output(
        ['pkg-config', '--libs', *PACKAGES], cwd=ROOT, env=environment, text=True, timeout=30))
    libraries += shlex.split(subprocess.check_output(
        ['bash', 'test/run.sh', '--native-link-flags'], cwd=ROOT, env=environment,
        text=True, timeout=30))
    binary = output / 'tree-representation-cost'
    source = ROOT / 'test/scripts/tree-representation-cost.cpp'
    command = [*compiler_command(), str(source), 'build/libgenerators.a', 'build/liboutshine.a',
               *libraries, '-lz', '-lcurl',
               '-o', str(binary)]
    checked(command)
    with (output / 'cost.csv').open('w') as report:
        checked([str(binary), *map(str, species)], stdout=report)
    with (output / 'cost.csv').open() as report:
        rows = list(csv.DictReader(report))
    if len(rows) != len(species) * 4:
        raise RuntimeError('incomplete four-level tree comparison')
    metadata = {'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                'inputs': {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in species},
                'compile': command, 'scope': 'CPU logical arrays; no capacities, allocator, GPU or RSS',
                'timing': 'one run, expanded before instanced; not a stable benchmark'}
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(f'{len(rows)} native comparisons: {output / "cost.csv"}')


if __name__ == '__main__':
    main()
