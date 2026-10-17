#!/usr/bin/env python3
"""Build and run the common native calibration against one matching engine archive.

Ten fixed workloads: one/two-worker four-lane FP32 FMA; uint32 sequential triads
with 768 KiB/96 MiB active arrays; dependent shuffled cycles with 64 KiB/64 MiB;
matching native GPU FMA/triads and a 1280x720 RGBA8 fullscreen integer raster.
One warmup and five measured samples per workload. Complete result verification,
input/setup/upload/readback and checksumming are outside the measured intervals.
CPU triad calls live in a separate TU, with no LTO; all compilation targets the
host CPU explicitly. CpuKernels.s records the actual generated FMA/load code.

GPU time comes only from native submission timestamps. Host encode, submit and
wait remain separate, unavailable timestamps become null. Small sets include
cache effects; logical bytes are not physical DRAM traffic. The working set is
active kernel data, excluding validation buffers; whole-process RSS includes
setup, CPU references, driver/readback allocations and the bounded supervisor.
Neither is a device-memory peak. Verification transfers occur between samples.
The CPU name identifies the host independently from its GPU. RAM is system total,
not an application budget. This is neither official STREAM nor a hardware peak.

The source is a portable Run(Device, Shaders, Build) function. This experiment
compiles only that function and its standalone caller against an existing full
engine archive and the exact same checkout's headers/link flags. Master owns
the later one-time product integration; no compiler/Python is needed at runtime.
All outputs are private under /tmp. No scenario, places, network, product CLI
or renderer-port changes are part of this delivery.
"""

import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import shlex
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def git(root, *args):
    return subprocess.check_output(['git', *args], cwd=root, text=True).strip()


def command(args, root, log):
    log.write(shlex.join(map(str, args)) + '\n')
    log.flush()
    subprocess.run(args, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)


def build(root, output, host_load):
    archive = root / 'build/liboutshine.a'
    compiler = shlex.split(os.environ.get('CXX', 'c++'))
    declarations = dict(line.split('=', 1) for line in (root / 'test/run.sh').read_text().splitlines()
                        if line.startswith(('CXXSTD=', 'WARN=', 'OPT=')))
    flags = [flag for key in ('CXXSTD', 'WARN', 'OPT')
             for flag in shlex.split(declarations[key].strip('"'))]
    cpu_flag = '-mcpu=native' if platform.machine().lower() in ('arm64', 'aarch64') else '-march=native'
    flags += ['-fno-exceptions', cpu_flag]
    includes = ['-I' + str(root), '-I' + str(root / 'include'), '-I' + str(HERE)]
    includes += ['-I' + str(path) for path in sorted((root / 'src').rglob('*')) if path.is_dir()]
    includes += shlex.split(subprocess.check_output(['pkg-config', '--cflags', 'sdl3'], text=True))
    libraries = shlex.split(subprocess.check_output(
        ['sh', 'test/run.sh', '--native-link-flags'], cwd=root, text=True))
    sources = sorted(HERE.glob('*.cpp'))
    calibration_root = HERE.parents[3]
    manifest = dict(engine_root=str(root), engine_commit=git(root, 'rev-parse', 'HEAD'),
                    engine_dirty=git(root, 'status', '--porcelain'),
                    archive_sha256=digest(archive), source_commit=git(calibration_root, 'rev-parse', 'HEAD'),
                    source_dirty=git(calibration_root, 'status', '--porcelain'),
                    sources={p.name: digest(p) for p in sorted(HERE.iterdir()) if p.is_file()},
                    compiler=compiler, flags=flags, host_load=host_load, status='building')
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    with (output / 'build.log').open('w') as log:
        objects = []
        for source in sources:
            obj = output / (source.stem + '.o')
            command(['nice', '-n', '10', *compiler, *flags, *includes, '-c', str(source), '-o', str(obj)], root, log)
            objects.append(str(obj))
        command(['nice', '-n', '10', *compiler, *flags, *objects, str(archive), *libraries,
                 '-o', str(output / 'calibration')], root, log)
        command(['nice', '-n', '10', *compiler, *flags, *includes, '-S', str(HERE / 'CpuKernels.cpp'),
                 '-o', str(output / 'CpuKernels.s')], root, log)
        glslang = shlex.split(os.environ.get('GLSLANG', 'glslangValidator'))
        for name in ('fma.comp', 'triad.comp', 'raster.vert', 'raster.frag'):
            command(['nice', '-n', '10', *glslang, '-V', '--target-env', 'vulkan1.0',
                     '-I' + str(HERE), str(HERE / name), '-o', str(output / (name + '.spv'))], root, log)
    manifest.update(status='built', binary_sha256=digest(output / 'calibration'),
                    shader_sha256={p.name: digest(p) for p in sorted(output.glob('*.spv'))})
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def run(root, output, manifest, host_load):
    for path, expected in [(output / 'calibration', manifest['binary_sha256']),
                           (root / 'build/liboutshine.a', manifest['archive_sha256'])]:
        if digest(path) != expected:
            raise ValueError(f'build artifact changed: {path}')
    for name, expected in manifest['shader_sha256'].items():
        if digest(output / name) != expected:
            raise ValueError(f'shader changed: {name}')
    args = [str(output / 'calibration'),
            *[str(output / (name + '.spv')) for name in ('fma.comp', 'triad.comp', 'raster.vert', 'raster.frag')],
            manifest['source_commit'], shlex.join(manifest['flags']), host_load,
            '1' if os.environ.get('OUTSHINE_CALIBRATION_VALIDATION') == '1' else '0']
    marker = output / 'timeout'
    for path in (marker, Path(str(marker) + '.start'), Path(str(marker) + '.cleanup-failed')):
        if path.exists():
            path.unlink()
    began = time.monotonic()
    environment = os.environ.copy()
    if sys.platform == 'darwin' and os.environ.get('OUTSHINE_CALIBRATION_VALIDATION') == '1':
        environment['MTL_DEBUG_LAYER'] = '1'
    with (output / 'supervisor.log').open('w') as log:
        child = subprocess.Popen([sys.executable, str(root / 'test/scripts/run_with_timeout.py'),
                                  str(os.getpid()), '120', str(output / 'run.log'), str(marker),
                                  '--stderr-file', str(output / 'stderr.log'), *args],
                                 cwd=root, stdout=log, stderr=subprocess.STDOUT, env=environment)
        Path(str(marker) + '.start').touch()
        _, status, usage = os.wait4(child.pid, 0)
        child.returncode = os.waitstatus_to_exitcode(status)
    status = child.returncode
    manifest.update(status='terminal', exit=status, process_seconds=time.monotonic() - began,
                    host_load=host_load, timed_out=marker.exists(),
                    cleanup_failed=Path(str(marker) + '.cleanup-failed').exists(),
                    whole_process_peak_rss_bytes=usage.ru_maxrss * (1 if sys.platform == 'darwin' else 1024),
                    user_seconds=usage.ru_utime, system_seconds=usage.ru_stime)
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    if status or manifest['timed_out'] or manifest['cleanup_failed']:
        raise RuntimeError(f'calibration failed or reported diagnostics: {output}')
    messages = (output / 'stderr.log').read_text().splitlines()
    metal_notice = (sys.platform == 'darwin'
                    and environment.get('OUTSHINE_CALIBRATION_VALIDATION') == '1'
                    and len(messages) == 1
                    and messages[0].endswith(' Metal API Validation Enabled'))
    if messages and not metal_notice:
        raise RuntimeError(f'calibration reported diagnostics: {output / "stderr.log"}')
    report = json.loads((output / 'run.log').read_text())
    report['provenance'] = manifest
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(output=str(output), workloads=len(report['workloads']),
                         cpu=report['cpu'], gpu=report['gpu'], seconds=manifest['process_seconds'])))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--host-load', required=True)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--build-only', action='store_true')
    modes.add_argument('--run-only', action='store_true')
    args = parser.parse_args()
    root, output = args.engine_root.resolve(), args.output.resolve()
    if not output.is_relative_to(Path('/tmp').resolve()):
        parser.error('private outputs must be under /tmp')
    output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((output / 'manifest.json').read_text()) if args.run_only else build(root, output, args.host_load)
    if not args.build_only:
        run(root, output, manifest, args.host_load)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
