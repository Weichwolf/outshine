"""Run the first shared native GPU consumer on the declared RTX 3060 host."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validation', action='store_true')
    args = parser.parse_args()
    validation = os.environ.get('VULKAN_VALIDATION', '')
    if validation not in ('', '0', '1'):
        raise ValueError('VULKAN_VALIDATION must be 0 or 1')
    args.validation = args.validation or validation == '1'
    output = ROOT / 'build/experiments/render/native-vulkan'
    output.mkdir(parents=True, exist_ok=True)
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True))
    log_path = Path(tempfile.gettempdir()) / f'outshine-vulkan-{commit[:9]}.log'
    with log_path.open('w') as log:
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        glslang = shlex.split(os.environ.get('GLSLANG', 'glslangValidator'))
        native = sorted((ROOT / 'src/render/device/native/vulkan').glob('*.cpp'))
        source = ROOT / 'test/harness/render/NativeVulkanProbe.cpp'
        flags = shlex.split(subprocess.check_output(
            ['pkg-config', '--cflags', '--libs', 'vulkan', 'spirv-cross-c-shared'],
            cwd=ROOT, text=True))
        binary = output / 'native_vulkan'
        run(['nice', '-n', '10', *compiler, '-std=c++23', '-O2', '-fno-exceptions',
             '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I.',
             *map(str, native), str(source), *flags, '-o', str(binary)], log)
        shaders = []
        shader_sources = []
        for name in ('subjectScanKernel', 'subjectCompactKernel'):
            shader_source = ROOT / f'src/render/shaders/{name}.comp'
            shader = output / f'{name}.comp.spv'
            run(['nice', '-n', '10', *glslang, '-V', '--target-env', 'vulkan1.0',
                 '-Isrc', '-Isrc/content/shade', str(shader_source), '-o', str(shader)], log)
            shaders.append(shader)
            shader_sources.append(shader_source)
        environment = os.environ.copy()
        arguments = [str(binary), *map(str, shaders)]
        if args.validation:
            arguments.append('--validation')
            enabled = environment.get('VK_LAYER_ENABLES', '').split(',')
            enabled.append('VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT')
            environment['VK_LAYER_ENABLES'] = ','.join(dict.fromkeys(filter(None, enabled)))
        result = run(arguments, log, environment=environment, timeout=20)
        if result.stderr:
            raise RuntimeError(f'native Vulkan probe reported diagnostics (see {log.name})')
        report = json.loads(result.stdout)
        report.update(commit=commit, dirty=dirty, log=str(log_path))
        report['sources'] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                             for path in [*native, source, *shader_sources]}
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
