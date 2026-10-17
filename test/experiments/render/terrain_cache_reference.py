"""Inspect one native cached ground region without fetching or modifying world inputs."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

from terrain_seams import analyse

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'test/scripts/terrain-region-export.cpp'


def compile_helper(output, source=SOURCE, unit="TerrainMesh.cpp"):
    records = json.loads((ROOT / 'compile_commands.json').read_text())
    record = next(row for row in records if Path(row['file']).name == unit)
    flags = record.get('arguments') or shlex.split(record['command'])
    command, skip = [], False
    for flag in flags:
        if skip:
            skip = False
        elif flag == '-o':
            skip = True
        elif flag != '-c' and Path(flag) != Path(record['file']):
            command.append(flag)
    environment = os.environ.copy()
    prefix = ROOT / 'build/deps/install/lib'
    environment['PKG_CONFIG_PATH'] = str(prefix / 'pkgconfig') + os.pathsep + environment.get('PKG_CONFIG_PATH', '')
    packages = ['sdl3', 'sdl3-image', 'sdl3-ttf', 'sdl3-shadercross', 'liblz4',
                'libtiff-4', 'libwebp', 'geos', 'sqlite3', 'libzstd', 'sfcgal', 'libcurl']
    libraries = shlex.split(subprocess.check_output(
        ['pkg-config', '--libs', *packages], env=environment, text=True, timeout=30))
    binary = output / source.stem
    command += [str(source), str(ROOT / 'build/libgenerators.a'),
                str(ROOT / 'build/liboutshine.a'), *libraries, '-lz',
                f'-Wl,-rpath,{prefix}', '-o', str(binary)]
    subprocess.run(command, cwd=ROOT, check=True, timeout=120)
    return binary, command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache', type=Path)
    parser.add_argument('key')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    binary, command = compile_helper(args.output)
    result = subprocess.run([str(binary), str(args.cache), args.key, str(args.output)],
                            capture_output=True, text=True, check=True, timeout=60)
    report = analyse(args.output)
    report.update(asset_key=args.key, anchor=result.stdout.strip(), compile_command=command,
                  native_source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                  source_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip())
    (args.output / 'seams.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(anchor=report['anchor'], pages=report['pages'],
                         same_edges=report['checked_same_level_edges'],
                         mixed_edges=report['checked_mixed_level_edges'])))


if __name__ == '__main__':
    main()
