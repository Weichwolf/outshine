"""Replay material source/RGB-T/coverage through the actual native GLSL core."""

import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import subprocess
import tempfile

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
DTYPE = np.dtype([('source_coverage', '<f4', 4), ('distances', '<f4', 2), ('at', '<i4'), ('mode', '<u4')])


def cases():
    rng = np.random.default_rng(273)
    count = 4096
    source = rng.uniform(0, 8, (count, 3)).astype('<f4')
    through = rng.uniform(0, 1, (count, 3)).astype('<f4')
    absorption = rng.uniform(0, 1, (count, 3)).astype('<f4')
    background = rng.uniform(0, 10, (count, 3)).astype('<f4')
    distances = 10 ** rng.uniform(-3, 2, (count, 2)).astype('<f4')
    coverage = rng.uniform(0, 1, count).astype('<f4')
    fixed = list(itertools.product((0., .01, .5, 1.), (0., .001, 1., 20.), (1., 100., np.inf),
                                   ((0., 0., 0.), (1., 1., 1.), (.1, .5, .9))))
    for index, (alpha, thickness, distance, colour) in enumerate(fixed):
        coverage[index] = alpha
        distances[index] = thickness, distance
        absorption[index] = colour
        through[index] = (0, 0, 0) if index % 3 == 0 else (1, 1, 1) if index % 3 == 1 else (.2, .5, .9)
    source[len(fixed):len(fixed) + 8] = 0
    through[len(fixed):len(fixed) + 8] = 1
    distances[len(fixed):len(fixed) + 8, 0] = 0
    coverage[len(fixed):len(fixed) + 8] = np.linspace(0, 1, 8)
    return source, through, absorption, background, distances, coverage, len(fixed)


def prepare(palette, output):
    source, through, absorption, background, distances, coverage, fixed = cases()
    count = len(source)
    records = np.zeros(3 * count, DTYPE)
    for mode in range(3):
        rows = records[mode * count:(mode + 1) * count]
        rows['source_coverage'][:, :3] = source
        rows['source_coverage'][:, 3] = coverage
        rows['distances'] = distances
        rows['at'] = len(palette) + np.arange(count) * 9
        rows['mode'] = mode
    parameters = np.column_stack([through, absorption, background]).ravel()
    np.concatenate([palette, parameters]).astype('<f4').tofile(output / 'parameters.f32')
    records.tofile(output / 'probes.bin')
    # The reference evaluates covered/uncovered area separately, with powers for Beer attenuation.
    s, t, colour, behind, d, a = [x.astype(float) for x in (source, through, absorption, background, distances, coverage)]
    medium = np.ones_like(t)
    active = (d[:, 0] > 0) & np.isfinite(d[:, 1])
    medium[active] = np.maximum(colour[active], 1e-5) ** (d[active, :1] / d[active, 1:])
    transmitted = t * medium
    first = np.column_stack([s, np.zeros(count), transmitted, np.zeros(count)])
    covered = np.column_stack([a[:, None] * s, np.zeros(count),
                               (1 - a[:, None]) + a[:, None] * transmitted, np.zeros(count)])
    resolved = (1 - a[:, None]) * behind + a[:, None] * (s + transmitted * behind)
    third = np.column_stack([resolved, np.zeros(count), medium, np.zeros(count)])
    # GLSL permits absolute log error near one; propagate that through the optical path.
    # Source: docs.vulkan.org/glsl/latest/chapters/variables.html, precision requirements.
    eps = np.finfo(np.float32).eps
    log_value = np.log(np.maximum(colour, 1e-5))
    ratio = np.zeros(len(d))
    ratio[active] = d[active, 0] / d[active, 1]
    argument = ratio[:, None] * log_value
    argument_error = ratio[:, None] * (2 ** -21 + 4 * eps * np.abs(log_value))
    argument_error += 8 * eps * (1 + np.abs(argument))
    upper = np.exp(argument + argument_error)
    medium_budget = medium * np.expm1(argument_error) + 8 * eps * (1 + np.abs(argument)) * upper
    medium_budget[~active] = 0
    medium_budget += np.finfo(np.float32).tiny
    zeros = np.zeros((count, 4))
    raw_budget = np.column_stack([zeros, t * medium_budget, np.zeros(count)])
    covered_budget = np.column_stack([zeros, a[:, None] * t * medium_budget, np.zeros(count)])
    resolved_budget = np.column_stack([a[:, None] * behind * t * medium_budget, np.zeros(count),
                                      medium_budget, np.zeros(count)])
    return np.concatenate([first, covered, third]), fixed, np.concatenate([raw_budget, covered_budget, resolved_budget])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--palette', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    assert output.is_relative_to(Path(tempfile.gettempdir()).resolve()) and not output.is_relative_to(ROOT)
    output.mkdir(parents=True, exist_ok=True)
    success = output / 'comparison.json'
    success.unlink(missing_ok=True)
    palette = np.fromfile(args.palette, '<f4')
    expected, fixed, precision_budget = prepare(palette, output)
    source = ROOT / 'test/experiments/render/surface_transfer_probe.comp'
    compile_command = ['nice', '-n', '10', os.environ.get('GLSLANG', 'glslangValidator'), '-V',
                       '--target-env', 'vulkan1.0', '-Isrc/render/shaders', str(source),
                       '-o', str(output / 'probe.spv')]
    compiled = subprocess.run(compile_command, cwd=ROOT, capture_output=True, text=True, timeout=60)
    (output / 'compile.log').write_text(compiled.stdout + compiled.stderr)
    assert compiled.returncode == 0
    command = [str(args.binary.resolve()), str(output / 'probe.spv'), str(output / 'parameters.f32'),
               str(output / 'probes.bin'), str(output / 'result.f32'), '13']
    run = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (output / 'native.log').write_text(run.stdout + run.stderr)
    assert run.returncode == 0, run.stderr
    actual = np.fromfile(output / 'result.f32', '<f4').reshape(-1, 8)
    assert np.isfinite(actual).all()
    error = np.abs(actual - expected)
    allowed = precision_budget + 3e-6 * (1 + np.abs(expected))
    assert np.all(error <= allowed), float(np.max(error / allowed))
    # Check the new transfer algebra tightly, independently of the measured old absorption.
    source_rgb, through, _, background, _, alpha, _ = cases()
    np.testing.assert_array_equal(actual[:4096, :3], source_rgb)
    covered_rows = actual[4096:2 * 4096]
    np.testing.assert_array_equal(covered_rows[alpha == 0, :3], np.zeros_like(covered_rows[alpha == 0, :3]))
    np.testing.assert_array_equal(covered_rows[alpha == 0, 4:7], np.ones_like(covered_rows[alpha == 0, 4:7]))
    np.testing.assert_array_equal(covered_rows[alpha == 1], actual[:4096][alpha == 1])
    measured_medium = actual[2 * 4096:, 4:7].astype(float)
    s, t, behind, a = [x.astype(float) for x in (source_rgb, through, background, alpha)]
    direct = np.column_stack([s, np.zeros(4096), t * measured_medium, np.zeros(4096)])
    covered = np.column_stack([a[:, None] * s, np.zeros(4096),
                               1 - a[:, None] + a[:, None] * t * measured_medium, np.zeros(4096)])
    resolved = (1 - a[:, None]) * behind + a[:, None] * (s + t * measured_medium * behind)
    np.testing.assert_allclose(actual[:2 * 4096], np.concatenate([direct, covered]), rtol=3e-6, atol=3e-6)
    np.testing.assert_allclose(actual[2 * 4096:, :3], resolved, rtol=3e-6, atol=3e-6)
    neutral = slice(2 * 4096 + fixed, 2 * 4096 + fixed + 8)
    _, _, _, background, _, _, _ = cases()
    np.testing.assert_array_equal(actual[neutral, :3], background[fixed:fixed + 8])
    paths = [source, ROOT / 'src/render/shaders/surfaceTransfer.glsl',
             ROOT / 'src/render/shaders/litFragment.glsl', ROOT / 'src/render/shaders/flatFragment.glsl', Path(__file__)]
    report = dict(gpu_executed=True, scope=__doc__, command=command, compile_command=compile_command,
                  exit=run.returncode, cases=4096, outputs=len(actual), neutral_identity='exact', coverage_extremes='exact',
                  max_absolute_error=float(np.max(np.abs(actual - expected))),
                  max_scaled_error=float(np.max(error / (1 + np.abs(expected)))),
                  max_precision_budget_fraction=float(np.max(error / allowed)),
                  precision_source='https://docs.vulkan.org/glsl/latest/chapters/variables.html',
                  absorption_scope='Existing GLSL exp/log precision, propagated through positive normal distances; not an image-quality bound',
                  transfer_scope='Independent area mixture with captured absorption, rtol/atol3e-6; exact neutral identity',
                  max_transfer_algebra_error=max(float(np.max(np.abs(actual[:2 * 4096] - np.concatenate([direct, covered])))),
                                                 float(np.max(np.abs(actual[2 * 4096:, :3] - resolved)))),
                  binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                  palette_sha256=hashlib.sha256(args.palette.read_bytes()).hexdigest(),
                  source_sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
    success.write_text(json.dumps(report, indent=2) + '\n')
    print('Actual native material transfer passed:', len(actual), 'outputs;', report['max_absolute_error'])


if __name__ == '__main__':
    main()
