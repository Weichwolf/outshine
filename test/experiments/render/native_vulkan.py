"""Exercise the production Vulkan archive on the declared RTX 3060 host."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def run(command, log, *, environment=None, timeout=120):
    result = subprocess.run(command, cwd=ROOT, env=environment, text=True,
                            capture_output=True, timeout=timeout, check=False)
    log.write(shlex.join(map(str, command)) + '\n')
    log.write(result.stdout)
    log.write(result.stderr)
    log.write(f'Exit: {result.returncode}\n')
    log.flush()
    if result.returncode:
        raise RuntimeError(f'native Vulkan command failed: {command[0]} (see {log.name})')
    return result


def verdict(result, log):
    if result.stderr:
        raise RuntimeError(f'native Vulkan probe reported diagnostics (see {log.name})')
    rows = re.findall(r'^CHECKS (\d+) FAILURES (\d+) SKIPPED (\d+) '
                      r'UNPREPARED (\d+) PARTIAL (\d+)$', result.stdout, re.MULTILINE)
    if len(rows) != 1 or any(int(value) for value in rows[0][1:]):
        raise RuntimeError(f'native Vulkan probe has an incomplete verdict (see {log.name})')
    return int(rows[0][0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != 'Linux':
        parser.error('the native Vulkan backend currently requires Linux')
    validation = os.environ.get('VULKAN_VALIDATION', '')
    if validation not in ('', '0', '1'):
        parser.error('VULKAN_VALIDATION must be 0 or 1')
    args.validation = args.validation or validation == '1'
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        parser.error('private outputs belong in the system temporary directory')
    archive = ROOT / 'build/liboutshine.a'
    if not archive.is_file():
        parser.error('build the production archive first with make all')
    output.mkdir(parents=True, exist_ok=True)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True))
    log_path = output / 'run.log'
    with log_path.open('w') as log:
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        glslang = shlex.split(os.environ.get('GLSLANG', 'glslangValidator'))
        flags = shlex.split(run(
            ['pkg-config', '--cflags', 'vulkan', 'spirv-cross-c-shared'], log).stdout)
        flags += shlex.split(run(['sh', 'test/run.sh', '--native-link-flags'], log).stdout)
        selected = run(['python3', 'test/scripts/source_units.py'], log).stdout.splitlines()
        native = [ROOT / name for name in selected if name.startswith('src/render/device/native/')]
        members = run(['ar', 't', str(archive)], log).stdout.splitlines()
        for unit in native:
            prefix = str(unit.relative_to(ROOT).with_suffix('')).replace('/', '-') + '.'
            if sum(member.startswith(prefix) for member in members) != 1:
                raise RuntimeError(f'archive does not contain exactly one selected object for {unit}')
        sources = [ROOT / 'src/render/shaders/subjectScanKernel.comp',
                   ROOT / 'src/render/shaders/subjectCompactKernel.comp',
                   Path(__file__).with_name('native_texture.comp'),
                   ROOT / 'src/render/shaders/depthPyramid.comp',
                   ROOT / 'src/render/shaders/fullscreen.vert',
                   Path(__file__).with_name('native_metal_mrt.frag'),
                   ROOT / 'src/render/shaders/present.frag',
                   Path(__file__).with_name('native_buffer_lengths.comp'),
                   Path(__file__).with_name('native_draw_transitions.vert'),
                   Path(__file__).with_name('native_draw_transitions.frag'),
                   Path(__file__).with_name('native_draw_indirect.comp'),
                   Path(__file__).with_name('native_draw.vert'),
                   Path(__file__).with_name('native_draw.frag')]
        shaders = []
        for source in sources:
            shader = output / (source.name + '.spv')
            run(['nice', '-n', '10', *glslang, '-V', '--target-env', 'vulkan1.0',
                 '-Isrc', '-Isrc/content/shade', '-Isrc/render/shaders', str(source), '-o', str(shader)], log)
            shaders.append(shader)
        consumers = [ROOT / 'test/harness/render' / name for name in
                     ('NativeVulkanProbe.cpp', 'NativeTextureProbe.cpp',
                      'NativeTextureLifetimeProbe.cpp', 'NativeRasterProbe.cpp',
                      'NativeRenderPassProbe.cpp', 'NativeMrtDrawProbe.cpp', 'NativeDrawTransitionsProbe.cpp',
                      'NativeDrawProbe.cpp', 'NativeMrtProbe.cpp')]
        binaries = []
        for source in consumers:
            binary = output / source.stem
            run(['nice', '-n', '10', *compiler, '-std=c++23', '-O2', '-fno-exceptions',
                 '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I.',
                 '-Isrc/render/device/native', '-Itest/harness/shared', str(source),
                 str(archive), *flags, '-o', str(binary)], log)
            binaries.append(binary)
        environment = os.environ.copy()
        suffix = ['--validation'] if args.validation else []
        if args.validation:
            enabled = environment.get('VK_LAYER_ENABLES', '').split(',')
            enabled.append('VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT')
            environment['VK_LAYER_ENABLES'] = ','.join(dict.fromkeys(filter(None, enabled)))
        result = run([str(binaries[0]), *map(str, shaders[:2]), *suffix], log,
                     environment=environment, timeout=20)
        if result.stderr:
            raise RuntimeError(f'native Vulkan probe reported diagnostics (see {log.name})')
        report = json.loads(result.stdout)
        report['texture_checks'] = verdict(run(
            [str(binaries[1]), *map(str, shaders[2:4]), *suffix], log,
            environment=environment, timeout=25), log)
        report['texture_lifetime_checks'] = verdict(run(
            [str(binaries[2]), str(shaders[2]), *suffix], log,
            environment=environment, timeout=25), log)
        raster_jobs = [('raster_pipeline_checks', 3, [4, 5, 6]),
                       ('render_pass_checks', 4, [7]),
                       ('mrt_draw_checks', 5, [4, 5]),
                       ('draw_transition_checks', 6, [8, 9, 10]),
                       ('draw_checks', 7, [11, 12]),
                       ('mrt_hiz_checks', 8, [4, 5, 3])]
        for key, binary, indices in raster_jobs:
            report[key] = verdict(run(
                [str(binaries[binary]), *(str(shaders[index]) for index in indices), *suffix],
                log, environment=environment, timeout=25), log)
        report.update(commit=commit, dirty=dirty, log=str(log_path),
                      archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                      native_archive_objects=len(native))
        headers = list((ROOT / 'src/render/device/native').glob('*.h'))
        headers += list((ROOT / 'src/render/device/native/vulkan').glob('*.h'))
        report['sources'] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                             for path in [*native, *headers, *consumers, *sources]}
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: value for key, value in report.items() if key != 'sources'}))


if __name__ == '__main__':
    main()
