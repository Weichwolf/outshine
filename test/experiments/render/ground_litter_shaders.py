"""Compile the actual ground filter candidates without creating a GPU device.

Requires the prepared BRDF tables, glslangValidator and SPIRV-Cross.
SPIR-V/MSL generation and reflected layouts are not native
execution, Metal compilation, GPU cost or a Place-quality proof.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[3]


def reflection_layout(document):
    types = document['types']

    def resolved(type_name):
        if type_name not in types:
            return type_name
        source = types[type_name]
        return dict(name=source.get('name'), members=[
            dict({key: value for key, value in member.items() if key != 'type'},
                 type=resolved(member['type'])) for member in source.get('members', [])])

    resources = []
    for kind in ('ssbos', 'ubos', 'textures', 'images', 'separate_images', 'separate_samplers'):
        for resource in document.get(kind, []):
            resources.append(dict(kind=kind, **{key: value for key, value in resource.items()
                                               if key != 'type'}, type=resolved(resource['type'])))
    return dict(entries=document['entryPoints'],
                resources=sorted(resources, key=lambda row: (row['kind'], row.get('set', 0), row.get('binding', 0))),
                inputs=document.get('inputs', []), outputs=document.get('outputs', []))


def check_probe(layout):
    assert layout['entries'][0]['workgroup_size'] == [64, 1, 1]
    resources = {row['name']: row for row in layout['resources']}
    assert set(resources) == {'Palette', 'Inputs', 'Outputs', 'Pushed'}
    assert resources['Palette']['readonly'] and resources['Inputs']['readonly']
    assert resources['Outputs']['writeonly']
    assert [(resources[name]['set'], resources[name]['binding'])
            for name in ('Palette', 'Inputs', 'Outputs', 'Pushed')] == [(0, 0), (0, 1), (1, 0), (2, 0)]
    assert resources['Palette']['type']['members'][0]['array_stride'] == 4
    for name, offsets in [('Inputs', [0, 16, 24, 28]), ('Outputs', [0, 16])]:
        array = resources[name]['type']['members'][0]
        assert array['array_stride'] == 32 and array['offset'] == 0
        assert [member['offset'] for member in array['type']['members']] == offsets
    assert resources['Pushed']['block_size'] == 16
    assert [member['offset'] for member in resources['Pushed']['type']['members']] == [0, 4, 8]


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
    prefix = [translator, str(spirv)]
    run([*prefix, '--reflect', '--output', str(metadata)], output, name + '-reflect')
    msl = output / (name + '.msl')
    run([*prefix, '--msl', '--msl-version', '20300', '--output', str(msl)], output, name + '-msl')
    assert msl.stat().st_size > 0
    pattern = r'([A-Za-z_]\w*(?:<[^<>]*>)?)[&*]?\s+\w+\s+\[\[(buffer|texture|sampler)\((\d+)\)\]\]'
    bindings = sorted(re.findall(pattern, msl.read_text()))
    assert bindings, name
    return dict(variant=variant, points=points, shape=reflection_layout(json.loads(metadata.read_text())),
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
    glslang = shutil.which(os.environ.get('GLSLANG', 'glslangValidator'))
    translator = shutil.which(os.environ.get('SPIRVCROSS', 'spirv-cross'))
    assert glslang and translator, 'install glslangValidator and spirv-cross or set GLSLANG/SPIRVCROSS'
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
        print(points, len(variants), 'SPIR-V/MSL variants, layouts and translated slots pass', flush=True)
    check_probe(baseline['probe'][0])
    source = ROOT / 'test/experiments/render/ground_litter_probe.comp'
    default = output / 'default.spv'
    run([glslang, '-V', '--target-env', 'vulkan1.0', '-S', 'comp',
         '-Isrc', '-Isrc/content/shade', '-Isrc/render/shaders', '-Ibuild/shaders',
         str(source), '-o', str(default)], output, 'default-compile')
    assert default.read_bytes() == (output / '3-probe.spv').read_bytes()
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
                  default_matches_explicit_nine_spirv=True,
                  native_syntax=native,
                  source_sha256={name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                                 for name in paths},
                  tools={tool: hashlib.sha256(Path(tool).read_bytes()).hexdigest()
                         for tool in (glslang, translator)})
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('invalid configuration rejected; no GPU device created')


if __name__ == '__main__':
    main()
