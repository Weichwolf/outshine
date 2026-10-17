#!/usr/bin/env python3
"""Check missing timestamps and reject a deliberately incorrect real GPU output."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='existing built calibration directory')
    args = parser.parse_args()
    root, output = args.engine_root.resolve(), args.output.resolve()
    if not output.is_relative_to(Path('/tmp').resolve()):
        parser.error('private outputs must be under /tmp')
    manifest = json.loads((output / 'manifest.json').read_text())
    includes = ['-I' + str(HERE)]
    includes += ['-I' + str(path) for path in sorted((root / 'src').rglob('*')) if path.is_dir()]
    compiler = [*manifest['compiler'], *manifest['flags'], *includes]
    binary = output / 'json-probe'
    with (output / 'checks-build.log').open('w') as log:
        subprocess.run(['nice', '-n', '10', *compiler, str(HERE / 'checks/JsonProbe.cpp'),
                        str(output / 'Json.o'), '-o', str(binary)], check=True, stdout=log,
                       stderr=subprocess.STDOUT, timeout=120)
        shader = output / 'wrong-fma.comp'
        shader.write_text((HERE / 'fma.comp').read_text().replace('into.values[at] = value;',
                                                               'into.values[at] = vec4(0.0);'))
        glslang = shlex.split(os.environ.get('GLSLANG', 'glslangValidator'))
        subprocess.run(['nice', '-n', '10', *glslang, '-V', '--target-env', 'vulkan1.0',
                        '-I' + str(HERE), str(shader), '-o', str(output / 'wrong-fma.spv')],
                       check=True, stdout=log, stderr=subprocess.STDOUT, timeout=120)
    result = json.loads(subprocess.check_output([str(binary)], text=True, timeout=10))
    assert result['cpu'] == 'quote" newline\n tab\t slash\\'
    missing, cpu = result['workloads']
    assert missing['summary'] is None
    assert all(sample['gpu_ns'] is None for sample in missing['samples'])
    assert cpu['summary']['median_ns'] == 20
    assert cpu['summary']['min_ns'] == 10 and cpu['summary']['max_ns'] == 30
    assert cpu['summary']['ns_per_access'] == 10
    environment = os.environ.copy()
    if sys.platform == 'darwin':
        environment['MTL_DEBUG_LAYER'] = '1'
    child = subprocess.run([str(output / 'calibration'), str(output / 'wrong-fma.spv'),
                            *[str(output / (name + '.spv')) for name in ('triad.comp', 'raster.vert', 'raster.frag')],
                            manifest['source_commit'], 'negative-control', 'functional check', '1'],
                           capture_output=True, text=True, timeout=120, env=environment)
    (output / 'negative-control.log').write_text(child.stdout + child.stderr)
    assert child.returncode != 0
    assert 'GPU calibration result differs from complete CPU reference' in child.stderr
    assert not child.stdout.strip()
    (output / 'checks.json').write_text(json.dumps(dict(status='terminal', exit=0,
                                                      missing_gpu_time='null without wait-time fallback',
                                                      json_escape='pass', real_gpu_mismatch='rejected'), indent=2) + '\n')
    print('JSON time-basis checks and real incorrect-GPU-output negative control passed')


if __name__ == '__main__':
    main()
