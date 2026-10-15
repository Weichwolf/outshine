"""Compile the actual ground filter candidates without creating a GPU device.

Requires the prepared BRDF tables, glslangValidator and the existing shadercross
offline translator. SPIR-V/MSL generation and reflected counts are not native
execution, Metal compilation, GPU cost or a Place-quality proof.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[3]


def run(command, output, name, expected=0):
    result = subprocess.run(['nice', '-n', '10', *command], cwd=ROOT,
                            capture_output=True, text=True, timeout=60)
    (output / (name + '.log')).write_text(result.stdout + result.stderr)
    if result.returncode != expected:
        raise RuntimeError(f'{name}: exit {result.returncode}: {result.stdout}{result.stderr}')
    return result


def compile_variant(output, points, variant, glslang, translator):
    stage = 'comp' if variant == 'probe' else 'frag'
    name = f'{points}-{variant}'
    spirv = output / (name + '.spv')
    source = ROOT / ('test/experiments/render/ground_litter_probe.comp' if stage == 'comp'
                     else 'src/render/shaders/groundLit.glsl')
    defines = [f'-DGROUND_LITTER_GAUSS_POINTS={points}']
    if stage == 'frag':
        flags = ('SUBJECT_WRITES_VELOCITY', 'SUBJECT_NORMAL_LOCATION', 'SUBJECT_IDENTITY_LOCATION',
                 'SUBJECT_BASE_LOCATION', 'SUBJECT_METALROUGH_LOCATION')
        defines += [f'-D{flag}={variant[i] if i < len(variant) else 0}'
                    for i, flag in enumerate(flags)]
    run([glslang, '-V', '--target-env', 'vulkan1.0', '-S', stage,
         '-Isrc', '-Isrc/content/shade', '-Isrc/render/shaders', '-Ibuild/shaders',
         *defines, str(source), '-o', str(spirv)], output, name + '-compile')
    metadata = output / (name + '.json')
    prefix = [translator, str(spirv), '-s', 'SPIRV', '-t', 'compute' if stage == 'comp' else 'fragment']
    run([*prefix, '-d', 'JSON', '-o', str(metadata)], output, name + '-reflect')
    msl = output / (name + '.msl')
    run([*prefix, '-d', 'MSL', '--msl-version', '2.3.0', '-o', str(msl)], output, name + '-msl')
    assert msl.stat().st_size > 0
    pattern = r'([A-Za-z_]\w*(?:<[^<>]*>)?)[&*]?\s+\w+\s+\[\[(buffer|texture|sampler)\((\d+)\)\]\]'
    bindings = sorted(re.findall(pattern, msl.read_text()))
    assert bindings, name
    return dict(variant=variant, points=points, shape=json.loads(metadata.read_text()),
                msl_resource_type_indices=bindings,
                spirv_bytes=spirv.stat().st_size,
                spirv_sha256=hashlib.sha256(spirv.read_bytes()).hexdigest(),
                msl_sha256=hashlib.sha256(msl.read_bytes()).hexdigest())


def native_syntax(output):
    declarations = dict(line.split('=', 1) for line in (ROOT / 'test/run.sh').read_text().splitlines()
                        if line.startswith(('CXXSTD=', 'WARN=', 'OPT=')))
    flags = [flag for key in ('CXXSTD', 'WARN', 'OPT')
             for flag in shlex.split(declarations[key].strip('"'))]
    include = Path(os.environ.get('NATIVE_INCLUDE', str(ROOT / 'src/render/device')))
    command = [*shlex.split(os.environ.get('CXX', 'c++')), *flags, '-fno-exceptions',
               '-I' + str(include), '-fsyntax-only', 'test/harness/render/NativeGroundLitterProbe.cpp']
    run(command, output, 'native-syntax')
    return dict(command=command, exit=0,
                header_sha256={name: hashlib.sha256((include / 'native' / name).read_bytes()).hexdigest()
                               for name in ('Device.h', 'Commands.h', 'Compute.h', 'Resources.h')})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--native-syntax', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error('use a directory in system temp')
    output.mkdir(parents=True, exist_ok=True)
    native = native_syntax(output) if args.native_syntax else None
    glslang = os.environ.get('GLSLANG', str(ROOT / 'build/deps/install/bin/glslangValidator'))
    translator = os.environ.get('SHADERCROSS', str(ROOT / 'build/deps/install/bin/shadercross'))
    result = run(['make', '--no-print-directory', '-s', '--eval',
                  'print-material-outputs:;@echo $(GROUND_OUTPUTS)', 'print-material-outputs'],
                 output, 'outputs')
    variants = ['probe', *result.stdout.split()]
    assert len(variants) == len(set(variants)) and len(variants) > 2
    assert (ROOT / 'build/shaders/brdfTables.glsl').is_file(), 'prepare BRDF tables with make first'
    records, baseline = [], {}
    for points in (0, 2, 3):
        for variant in variants:
            record = compile_variant(output, points, variant, glslang, translator)
            layout = record['shape'], record['msl_resource_type_indices']
            if points == 0:
                baseline[variant] = layout
            else:
                assert layout == baseline[variant], (points, variant)
            records.append(record)
        print(points, len(variants), 'SPIR-V/MSL variants and reflected counts pass', flush=True)
    assert baseline['probe'][0] == dict(samplers=0, readonly_storage_textures=0, readonly_storage_buffers=2,
                                        readwrite_storage_textures=0, readwrite_storage_buffers=1,
                                        uniform_buffers=1, threadcount_x=64, threadcount_y=1, threadcount_z=1)
    source = ROOT / 'test/experiments/render/ground_litter_probe.comp'
    default = output / 'default.spv'
    run([glslang, '-V', '--target-env', 'vulkan1.0', '-S', 'comp',
         '-Isrc', '-Isrc/content/shade', '-Isrc/render/shaders', '-Ibuild/shaders',
         str(source), '-o', str(default)], output, 'default-compile')
    assert default.read_bytes() == (output / '0-probe.spv').read_bytes()
    negative = subprocess.run(['nice', '-n', '10', glslang, '-V', '-DGROUND_LITTER_GAUSS_POINTS=4', '-Isrc/render/shaders',
                               str(source), '-o', str(output / 'invalid.spv')],
                              cwd=ROOT, capture_output=True, text=True, timeout=60)
    (output / 'invalid.log').write_text(negative.stdout + negative.stderr)
    message = ''.join((negative.stdout + negative.stderr).split())
    assert negative.returncode != 0 and 'mustbe0,2or3' in message
    assert not (output / 'invalid.spv').exists()
    paths = ['src/render/shaders/groundLitterOriented.glsl', 'src/render/shaders/groundLayers.glsl',
             'test/experiments/render/ground_litter_probe.comp', str(Path(__file__).relative_to(ROOT)),
             'test/harness/render/NativeGroundLitterProbe.cpp']
    report = dict(scope=__doc__, records=records, invalid_configuration_rejected=True,
                  default_matches_explicit_box_spirv=True,
                  native_syntax=native,
                  source_sha256={name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                                 for name in paths},
                  tools={tool: hashlib.sha256(Path(tool).read_bytes()).hexdigest()
                         for tool in (glslang, translator)})
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('invalid configuration rejected; no GPU device created')


if __name__ == '__main__':
    main()
