#!/usr/bin/env python3
"""Check dense ordered draws against one complete matching native engine checkout."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--validation', action='store_true')
    parser.add_argument('--heap-usage', action='store_true', help='separate glibc allocation diagnostic; not a timing run')
    args = parser.parse_args()
    root, output = args.checkout.resolve(), args.output.resolve()
    if not output.is_relative_to(Path('/tmp').resolve()):
        parser.error('private outputs must be under /tmp')
    output.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent
    source = here.parents[1] / 'harness/render/NativeDrawPacketProbe.cpp'
    declarations = dict(line.split('=', 1) for line in (root / 'test/run.sh').read_text().splitlines()
                        if line.startswith(('CXXSTD=', 'WARN=', 'OPT=')))
    flags = [flag for key in ('CXXSTD', 'WARN', 'OPT')
             for flag in shlex.split(declarations[key].strip('"'))]
    database = json.loads((root / 'compile_commands.json').read_text())
    entry = next(row for row in database if Path(row['file']).name == 'StagePipelines.cpp')
    includes = [flag for flag in entry.get('arguments', shlex.split(entry.get('command', '')))
                if flag.startswith('-I')]
    libraries = shlex.split(subprocess.check_output(
        ['sh', 'test/run.sh', '--native-link-flags'], cwd=root, text=True))
    compiler = shlex.split(os.environ.get('CXX', 'c++'))
    binary = output / 'NativeDrawPacketProbe'
    archive = root / 'build/liboutshine.a'
    command = [*compiler, *flags, '-fno-exceptions', '-I.', '-Isrc/render/device/native',
               '-Itest/harness/shared', '-Itest/harness/render', *includes, str(source),
               str(archive), *libraries, '-o', str(binary)]
    manifest = dict(checkout=str(root), commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                    cwd=root, text=True).strip(), dirty=subprocess.check_output(
                    ['git', 'status', '--porcelain'], cwd=root, text=True),
                    archive_sha256=digest(archive), probe_sha256=digest(source),
                    source_checkout=str(here.parents[2]), source_commit=subprocess.check_output(
                    ['git', 'rev-parse', 'HEAD'], cwd=here.parents[2], text=True).strip(),
                    source_dirty=subprocess.check_output(['git', 'status', '--porcelain'],
                    cwd=here.parents[2], text=True), compile_command=command,
                    validation=args.validation, heap_diagnostic=args.heap_usage,
                    status='building')
    (output / 'report.json').write_text(json.dumps(manifest, indent=2) + '\n')
    try:
        with (output / 'build.log').open('w') as log:
            subprocess.run(['nice', '-n', '10', *command], cwd=root, stdout=log,
                           stderr=subprocess.STDOUT, check=True, timeout=120)
            glslang = shlex.split(os.environ.get('GLSLANG', 'glslangValidator'))
            for stage in ('vert', 'frag'):
                subprocess.run(['nice', '-n', '10', *glslang, '-V', '--target-env', 'vulkan1.0',
                                str(here / ('native_draw_packets.' + stage)),
                                '-o', str(output / (stage + '.spv'))], cwd=root, stdout=log,
                               stderr=subprocess.STDOUT, check=True, timeout=120)
    except subprocess.SubprocessError as error:
        manifest.update(status='terminal', phase='build', exit=getattr(error, 'returncode', 1))
        (output / 'report.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print(f'build failed: {output / "build.log"}', file=sys.stderr)
        return 1
    manifest['shader_sources_sha256'] = {stage: digest(here / ('native_draw_packets.' + stage))
                                        for stage in ('vert', 'frag')}
    manifest['shader_spirv_sha256'] = {stage: digest(output / (stage + '.spv'))
                                      for stage in ('vert', 'frag')}
    environment = os.environ.copy()
    if args.heap_usage:
        library = Path('/usr/lib/x86_64-linux-gnu/libmemusage.so')
        if not library.is_file():
            parser.error('glibc libmemusage is unavailable; omit --heap-usage on other hosts')
        environment['LD_PRELOAD'] = str(library)
        environment['MEMUSAGE_OUTPUT'] = str(output / 'heap.bin')
        environment['MEMUSAGE_NO_TIMER'] = '1'
        environment['MEMUSAGE_PROG_NAME'] = binary.name
    args_run = [str(binary), str(output / 'vert.spv'), str(output / 'frag.spv')]
    if args.validation:
        args_run.append('--validation')
        if sys.platform == 'darwin':
            environment['MTL_DEBUG_LAYER'] = '1'
        enables = environment.get('VK_LAYER_ENABLES', '').split(',')
        environment['VK_LAYER_ENABLES'] = ','.join(dict.fromkeys(filter(None,
                     [*enables, 'VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT'])))
    manifest['run_command'] = args_run
    marker = output / 'timeout'
    for path in (marker, Path(str(marker) + '.start'), Path(str(marker) + '.cleanup-failed')):
        if path.exists():
            path.unlink()
    began = time.monotonic()
    with (output / 'supervisor.log').open('w') as log:
        child = subprocess.Popen([sys.executable, str(root / 'test/scripts/run_with_timeout.py'),
                    str(os.getpid()), '120', str(output / 'run.log'), str(marker), *args_run],
                    cwd=root, env=environment, stdout=log, stderr=subprocess.STDOUT)
        Path(str(marker) + '.start').touch()
        _, status, usage = os.wait4(child.pid, 0)
        child.returncode = os.waitstatus_to_exitcode(status)
    rows = (output / 'run.log').read_text().splitlines()
    verdicts = [re.fullmatch(r'CHECKS (\d+) FAILURES (\d+) SKIPPED (\d+) UNPREPARED (\d+) PARTIAL (\d+)', line)
                for line in rows if line.startswith('CHECKS ')]
    complete = len(verdicts) == 1 and verdicts[0] and not any(int(n) for n in verdicts[0].groups()[1:])
    devices = [line.removeprefix('DEVICE ') for line in rows if line.startswith('DEVICE ')]
    metal_notice = [line for line in rows if line.endswith(' Metal API Validation Enabled')]
    expected_notice = sys.platform == 'darwin' and args.validation and len(metal_notice) == 1
    ordinary = [line for line in rows if line.strip() and not (expected_notice and line in metal_notice)]
    clean = (args.heap_usage or len(ordinary) == 2) and not any('VUID-' in line or line.startswith('vulkan:') for line in rows)
    if sys.platform == 'darwin' and args.validation and not expected_notice:
        clean = False
    complete = complete and len(devices) == 1
    manifest.update(status='terminal', exit=child.returncode, process_seconds=time.monotonic() - began,
                    whole_process_peak_rss_bytes=usage.ru_maxrss * (1 if sys.platform == 'darwin' else 1024),
                    device=devices[0] if len(devices) == 1 else None, complete=bool(complete), diagnostics_absent=bool(clean), timed_out=marker.exists(),
                    cleanup_failed=Path(str(marker) + '.cleanup-failed').exists())
    (output / 'report.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({key: value for key, value in manifest.items() if key not in ('compile_command', 'dirty')}))
    return int(bool(child.returncode or not complete or not clean or manifest['timed_out'] or manifest['cleanup_failed']))


if __name__ == '__main__':
    sys.exit(main())
