#!/usr/bin/env python3
"""Compare complete production height uploads using each checkout's own SDK and headers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout', type=Path, required=True)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--scalar', action='store_true')
    args = parser.parse_args()
    root, output, source = args.checkout.resolve(), args.output.resolve(), Path(__file__).with_suffix('.cpp')
    if not output.is_relative_to(Path('/tmp').resolve()):
        parser.error('private output must be under /tmp')
    output.parent.mkdir(parents=True, exist_ok=True)
    database = json.loads((root / 'compile_commands.json').read_text())
    entry = next(row for row in database if Path(row['file']).name == 'Main.cpp'
                 and Path(row['file']).parent.name == 'client')
    command = entry.get('arguments') or shlex.split(entry['command'])
    command = [str(source) if part == entry['file'] else part
               for part in command if part != '-c']
    binary = output.with_suffix('.bin')
    packages = ['sdl3', 'sdl3-image', 'sdl3-ttf', 'liblz4', 'libtiff-4', 'libwebp',
                'geos', 'sqlite3', 'libzstd', 'sfcgal']
    libraries = shlex.split(subprocess.check_output(['pkg-config', '--libs', *packages], text=True))
    libraries += ['-lz', '-lcurl']
    libraries += shlex.split(subprocess.check_output(
        ['sh', 'test/run.sh', '--native-link-flags'], cwd=root, text=True))
    command += ['-I.', '-Itest/harness/render',
                f'-DOUTSHINE_HEIGHT_PAGE_BATCH={int(not args.scalar)}',
                *['-I' + str(path.relative_to(root)) for path in sorted((root / 'src').rglob('*'))
                  if path.is_dir()],
                'src/diagnostics/ProcessHeap.cpp', 'build/liboutshine.a', *libraries,
                '-o', str(binary)]
    manifest = {'checkout': str(root), 'commit': subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        'dirty': subprocess.check_output(['git', 'status', '--porcelain'], cwd=root, text=True),
        'probe_sha256': digest(source), 'input': str(args.input.resolve()),
        'input_sha256': digest(args.input), 'sdk_sha256': digest(root / 'build/liboutshine.a'),
        'compile_command': command, 'status': 'building'}
    output.write_text(json.dumps(manifest, indent=2) + '\n')
    with output.with_suffix('.build.log').open('w') as log:
        compiled = subprocess.run(['nice', '-n', '10', *command], cwd=root,
                                  stdout=log, stderr=subprocess.STDOUT, timeout=180)
    manifest['compile_exit'] = compiled.returncode
    if compiled.returncode:
        manifest['status'] = 'terminal'
        output.write_text(json.dumps(manifest, indent=2) + '\n')
        return compiled.returncode
    began = time.monotonic()
    marker = output.with_suffix('.timeout')
    for path in (marker, Path(str(marker) + '.start'), Path(str(marker) + '.cleanup-failed')):
        if path.exists():
            path.unlink()
    with output.with_suffix('.supervisor.log').open('w') as log:
        child = subprocess.Popen([sys.executable, str(root / 'test/scripts/run_with_timeout.py'),
                                  str(os.getpid()), '180', str(output.with_suffix('.run.log')),
                                  str(marker), str(binary), str(args.input.resolve())], cwd=root,
                                 stdout=log, stderr=subprocess.STDOUT)
        Path(str(marker) + '.start').touch()
        _, status, usage = os.wait4(child.pid, 0)
        child.returncode = os.waitstatus_to_exitcode(status)
    manifest.update(status='terminal', exit=child.returncode,
                    process_seconds=time.monotonic() - began,
                    whole_process_peak_rss_bytes=usage.ru_maxrss * (1 if sys.platform == 'darwin' else 1024),
                    user_seconds=usage.ru_utime, system_seconds=usage.ru_stime,
                    timed_out=marker.exists(),
                    cleanup_failed=Path(str(marker) + '.cleanup-failed').exists())
    lines = output.with_suffix('.run.log').read_text().splitlines()
    if child.returncode == 0:
        manifest['measurement'] = json.loads(next(line for line in reversed(lines) if line.startswith('{')))
    output.write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({key: value for key, value in manifest.items()
                      if key not in ('compile_command', 'dirty')}))
    return child.returncode


if __name__ == '__main__':
    sys.exit(main())
