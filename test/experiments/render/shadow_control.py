"""Isolate ground shadow sampling without changing engine sources or main shader artifacts."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['baseline', 'disabled', 'centered'], default='disabled')
    mode = parser.parse_args().mode
    output = ROOT/'build/experiments/render/shadow-control'/mode
    shaders = output/'build/shaders'
    shaders.mkdir(parents=True, exist_ok=True)
    shutil.copytree(ROOT/'build/shaders', shaders, dirs_exist_ok=True)
    source = output/'shader-source'
    source.mkdir(exist_ok=True)
    for name in ['groundLit.glsl', 'subjectLighting.glsl']:
        shutil.copyfile(ROOT/'src/render/shaders'/name, source/name)
    lighting = source/'subjectLighting.glsl'
    text = lighting.read_text()
    if mode == 'disabled':
        original = 'attenuation *= worldSunShadowVisibility(shadowMap, shadowCoordinate, receiverSlope);'
        assert text.count(original) == 1
        text = text.replace(original, 'attenuation *= 1.0;')
    elif mode == 'baseline':
        original = 'shadowReceiverVector(shadowCoordinate)'
        assert text.count(original) == 1
        text = text.replace(original, 'vec3(shadowCoordinate.xy * vec2(0.5, -0.5) + 0.5, shadowCoordinate.z)')
    lighting.write_text(text)
    if not (output/'src').exists():
        (output/'src').symlink_to(ROOT/'src', target_is_directory=True)
    compiler = ROOT/'build/deps/install/bin/glslangValidator'
    for artifact in sorted(shaders.glob('groundLit-*.frag.spv')):
        variant = artifact.name.removeprefix('groundLit-').removesuffix('.frag.spv')
        layout = variant.ljust(5, '0')
        definitions = ['SUBJECT_WRITES_VELOCITY', 'SUBJECT_NORMAL_LOCATION',
                       'SUBJECT_IDENTITY_LOCATION', 'SUBJECT_BASE_LOCATION',
                       'SUBJECT_METALROUGH_LOCATION']
        command = [str(compiler), '-V', '--target-env', 'vulkan1.0', '-S', 'frag',
                   '-I'+str(ROOT/'src'), '-I'+str(ROOT/'src/content/shade'),
                   '-I'+str(ROOT/'src/render/shaders'), '-I'+str(ROOT/'build/shaders')]
        command += ['-D'+name+'='+value for name, value in zip(definitions, layout)]
        command += [str(source/'groundLit.glsl'), '-o', str(artifact)]
        subprocess.run(command, cwd=output, check=True, capture_output=True, timeout=30)
    control = ROOT/'build/experiments/render/ground-detail/GroundGrains.scenario'
    log = Path(os.environ.get('TMPDIR', '/tmp'))/'outshine-ground-shadow-control.log'
    command = [str(ROOT/'build/outshine-client'), 'measures', '--offline', '--quality',
               'refined', '--view', 'station', '--at-seconds', '0', '--into', 'terrain-contact',
               str(control), 'GroundShadowControl']
    with log.open('w') as stream:
        subprocess.run(command, cwd=output, stdout=stream, stderr=subprocess.STDOUT,
                       check=True, timeout=180)
    image = output/'build/shots/terrain-contact/GroundShadowControl-station-tick1.png'
    shutil.copyfile(image, output/'control.png')
    image.unlink()
    (output/'control.json').write_text(json.dumps(dict(command=command, log=str(log),
        mode=mode, scope='Isolated ground-shadow comparison; source, geometry, material and light unchanged.'), indent=2)+'\n')
    print(output/'control.png')


if __name__ == '__main__':
    main()
