"""Run the existing ground component cases through the common native GPU probe.

Preparation alone creates inputs, compiled candidates and CPU control references;
it never reports a GPU pass. Execution requires the actual linked native probe.
No SDL GPU device or backend implementation is supplied by this experiment.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

import numpy as np

from ground_detail import noise2
from ground_litter_probe import ROOT, box_reference, make_probes, validate


def gauss_reference(at, dx, dy, scale, points):
    at, dx, dy = [value / scale[:, None] for value in (at, dx, dy)]
    width = np.maximum(np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1))
    field = np.zeros(len(at))
    active = (width > .01) & (width < 2)
    nodes, weights = np.polynomial.legendre.leggauss(points)
    for y, wy in zip(nodes * .5, weights * .5):
        for x, wx in zip(nodes * .5, weights * .5):
            field[active] += wx * wy * (noise2(at[active] + x * dx[active] + y * dy[active]) - .5)
    fade = np.clip(width - 1, 0, 1)
    field *= 1 - fade * fade * (3 - 2 * fade)
    near = width <= .01
    field[near] = noise2(at[near]) - .5
    return field


def palettes(original):
    native, legacy, neutral = [original.copy() for _ in range(3)]
    rows = int(original[:1].view('<u4')[0])
    layers = int(original[3:4].view('<u4')[0])
    surfaces = 4 * (rows + 2) + rows + 1
    assert rows > 0 and layers >= surfaces + 8 * (rows + 1)
    assert layers + 8 * (rows + 1) <= len(original)
    legacy[3] = 0
    for row in range(rows):
        neutral[surfaces + 8 * row + 3] = 0
        neutral[layers + 8 * row + 7] = 0
    return dict(native=native, legacy=legacy, neutral=neutral)


def prepare(output, original):
    variants = palettes(original)
    probes, count, panels = make_probes(original)
    probes.tofile(output / 'probes.bin')
    for name, palette in variants.items():
        palette.tofile(output / (name + '.palette'))
    rows = int(original[:1].view('<u4')[0])
    surfaces = 4 * (rows + 2) + rows + 1
    controls = probes[:count]
    indices = np.where((controls['which'] >= 0) & (controls['which'] < rows), controls['which'], rows - 1)
    scales = original[surfaces + 8 * indices + 2].astype(float)
    scales[scales <= 0] = 1
    arguments = (controls['point_dx'][:, :2].astype(float), controls['point_dx'][:, 2:].astype(float),
                 controls['dy'].astype(float), scales)
    manifest = dict(status='prepared_only', gpu_executed=False, probes=len(probes), controls=count,
                    unmapped=rows - 1, input_palette_sha256=hashlib.sha256(original.tobytes()).hexdigest(),
                    candidates=[])
    for points in (0, 2, 3):
        shader = output / f'filter-{points}.spv'
        command = ['nice', '-n', '10', os.environ.get('GLSLANG', 'glslangValidator'),
                   '-V', '--target-env', 'vulkan1.0', '-Isrc/render/shaders',
                   f'-DGROUND_LITTER_GAUSS_POINTS={points}',
                   'test/experiments/render/ground_litter_probe.comp', '-o', str(shader)]
        compiled = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
        (output / f'filter-{points}.log').write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode != 0:
            raise RuntimeError(f'shader compilation failed: {compiled.stdout}{compiled.stderr}')
        reference = box_reference(*arguments) if points == 0 else gauss_reference(*arguments, points)
        assert np.isfinite(reference).all() and np.max(np.abs(reference)) <= .5 + 1e-12
        reference.astype('<f8').tofile(output / f'filter-{points}-reference.f64')
        manifest['candidates'].append(dict(points=points, shader_sha256=hashlib.sha256(shader.read_bytes()).hexdigest(),
                                           command=command, compile_exit=compiled.returncode,
                                           field_range=[float(reference.min()), float(reference.max())]))
    manifest['source_sha256'] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in (Path(__file__), ROOT / 'test/experiments/render/ground_litter_probe.py',
                                              ROOT / 'src/render/shaders/groundLitterOriented.glsl')}
    (output / 'preparation.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return variants, probes, count, panels


def capture(binary, output, points, variant, count, unmapped):
    target = output / f'filter-{points}-{variant}.f32'
    target.unlink(missing_ok=True)
    command = [str(binary), str(output / f'filter-{points}.spv'), str(output / (variant + '.palette')),
               str(output / 'probes.bin'), str(target), str(unmapped)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
    log = result.stdout + result.stderr
    (output / f'filter-{points}-{variant}.log').write_text(log)
    if result.returncode != 0 or 'Validation Error' in log or 'VUID-' in log:
        raise RuntimeError(f'native capture failed ({result.returncode}): {log}')
    values = np.fromfile(target, dtype='<f4').reshape(-1, 8)
    assert len(values) == count and np.isfinite(values).all()
    return values


def execute(binary, output, original, variants, probes, count, panels):
    reports = []
    unmapped = int(original[:1].view('<u4')[0]) - 1
    for points in (0, 2, 3):
        captures = {name: capture(binary, output, points, name, len(probes), unmapped) for name in variants}
        reference = box_reference if points == 0 else lambda *args: gauss_reference(*args, points)
        description = '64x64 midpoint box' if points == 0 else f'NumPy Legendre {points}x{points} oriented quadrature'
        report = validate(original, probes, captures['native'], captures['legacy'], captures['neutral'],
                          count, reference, description)
        far = np.concatenate([np.arange(start, start + 128 * 96) for _, width, start in panels if width == 2])
        np.testing.assert_allclose(captures['native'][far], captures['legacy'][far], rtol=0, atol=1e-7)
        reports.append(dict(points=points, **report))
    (output / 'comparison.json').write_text(json.dumps(dict(gpu_executed=True,
                                                          binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                                                          results=reports), indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--palette', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--prepare-only', action='store_true')
    mode.add_argument('--binary', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error('use a directory in system temp')
    output.mkdir(parents=True, exist_ok=True)
    (output / 'comparison.json').unlink(missing_ok=True)
    original = np.fromfile(args.palette, dtype='<f4')
    variants, probes, count, panels = prepare(output, original)
    if args.binary:
        execute(args.binary.resolve(), output, original, variants, probes, count, panels)
    print('GPU comparison passed' if args.binary else 'prepared inputs and CPU references; GPU execution missing')


if __name__ == '__main__':
    main()
