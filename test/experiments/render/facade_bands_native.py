"""Check actual native facade-band parameters; outputs are not lit Places."""

import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import subprocess
import tempfile

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
DTYPE = np.dtype([('point_dx', '<f4', 4), ('dy', '<f4', 2), ('which', '<i4'), ('mode', '<u4')])


def rectangle_axis(at, width, low, high):
    width = np.maximum(width, .0001)
    at = np.remainder(at, 1)
    left, right = at - width / 2, at + width / 2
    area = np.zeros_like(at)
    for period in range(-int(np.ceil(width.max())) - 1, int(np.ceil(width.max())) + 2):
        area += np.maximum(0, np.minimum(right, period + high) - np.maximum(left, period + low))
    return area / width


def reference(probes, palette, profiles):
    style = np.where(probes['which'] < 0, -probes['which'] - 1, probes['which'])
    fractions = profiles[style]
    at = probes['point_dx'][:, :2].astype(float)
    width = np.abs(probes['point_dx'][:, 2:].astype(float)) + np.abs(probes['dy'].astype(float))
    floor_m = probes['mode'].view('<f4').astype(float)
    opening = np.prod([rectangle_axis(at[:, axis], width[:, axis], (1 - fractions[:, axis]) / 2,
                                      (1 + fractions[:, axis]) / 2) for axis in (0, 1)], axis=0)
    pane_x = rectangle_axis(at[:, 0], width[:, 0], (1 - fractions[:, 0]) / 2 + .02, .494)
    pane_x += rectangle_axis(at[:, 0], width[:, 0], .506, (1 + fractions[:, 0]) / 2 - .02)
    pane_y = rectangle_axis(at[:, 1], width[:, 1], (1 - fractions[:, 1]) / 2 + .02,
                            (1 + fractions[:, 1]) / 2 - .02)
    pane = pane_x * pane_y
    fraction = np.minimum(.16 / floor_m, 1 - fractions[:, 1])
    band = rectangle_axis(at[:, 1], width[:, 1], 0, fraction / 2)
    band += rectangle_axis(at[:, 1], width[:, 1], 1 - fraction / 2, 1)
    band = np.where(probes['which'] < 0, 0, np.minimum(band, 1 - opening))
    colour = ((1 - opening - .35 * band)[:, None] * palette[4:7]
              + (opening - pane)[:, None] * [.48, .46, .41] + pane[:, None] * [.025, .035, .045])
    roughness = (1 - opening) * palette[7] + (opening - pane) * .55 + pane * .12
    return np.column_stack([colour, roughness, opening, pane, band, 1 / floor_m])


def inputs():
    controls = list(itertools.product((1, 4, 8, 9, 10), (1., 2.4, 2.9, 4., 12.),
                                     (-4096.9, -2.47, 0., .03, .5, 4096.125),
                                     (.0002, .03, .5, 1., 16.), (False, True)))
    probes = np.zeros(2 * len(controls), DTYPE)
    for index, (style, floor_m, phase, width, skew) in enumerate(controls):
        for offset, which in ((0, style), (len(controls), -style - 1)):
            row = probes[index + offset]
            row['point_dx'] = [phase + .17, phase, width, .4 * width if skew else 0]
            row['dy'] = [-.3 * width if skew else 0, width]
            row['which'] = which
            row['mode'] = np.array(floor_m, '<f4').view('<u4')
    panels = []
    for style, floor_m, width in ((1, 2.4, .02), (8, 3.7, .02), (1, 2.4, 4.)):
        for before in (True, False):
            panel = np.zeros(320 * 160, DTYPE)
            x, y = np.meshgrid(np.linspace(0, 6, 320), np.linspace(0, 3, 160))
            panel['point_dx'][:, :2] = np.column_stack([x.ravel(), y.ravel()])
            panel['point_dx'][:, 2] = width
            panel['dy'][:, 1] = width
            panel['which'] = -style - 1 if before else style
            panel['mode'] = np.array(floor_m, '<f4').view('<u4')
            panels.append(panel)
    return np.concatenate([probes, *panels]), len(probes)


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
    probes, controls = inputs()
    probes.tofile(output / 'probes.bin')
    palette = np.fromfile(args.palette, '<f4')
    profiles = np.array([list(map(float, line.rstrip(')').split(',')[-2:]))
                         for line in (ROOT / 'src/content/shade/FacadeProfiles.inc').read_text().splitlines()])
    source = ROOT / 'test/experiments/render/facade_band_probe.comp'
    command = [os.environ.get('GLSLANG', 'glslangValidator'), '-V', '--target-env', 'vulkan1.0',
               '-S', 'comp', '-Isrc/render/shaders', '-Isrc', '-Isrc/content/shade',
               str(source), '-o', str(output / 'probe.spv')]
    compiled = subprocess.run(['nice', '-n', '10', *command], cwd=ROOT, capture_output=True, text=True, timeout=60)
    (output / 'compile.log').write_text(compiled.stdout + compiled.stderr)
    assert compiled.returncode == 0
    command = [str(args.binary.resolve()), str(output / 'probe.spv'), str(args.palette.resolve()),
               str(output / 'probes.bin'), str(output / 'result.f32'), '13']
    run = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (output / 'native.log').write_text(run.stdout + run.stderr)
    assert run.returncode == 0, run.stderr
    actual = np.fromfile(output / 'result.f32', '<f4').reshape(-1, 8)
    expected = reference(probes, palette, profiles)
    error = np.abs(actual - expected)
    assert np.isfinite(actual).all() and error.max() < .0002, error.max()
    canvas = Image.new('RGB', (640, 3 * 184), 'white')
    draw = ImageDraw.Draw(canvas)
    for index in range(6):
        colours = np.maximum(actual[controls + index * 51200:controls + (index + 1) * 51200, :3], 0)
        srgb = np.where(colours <= .0031308, 12.92 * colours, 1.055 * colours ** (1 / 2.4) - .055)
        pixels = np.uint8(np.clip(srgb * 255, 0, 255)).reshape(160, 320, 3)
        x, y = index % 2 * 320, index // 2 * 184
        canvas.paste(Image.fromarray(pixels), (x, y + 24))
        draw.text((x + 4, y + 5), ('Previous' if index % 2 == 0 else 'Metric bands') + ' - albedo only', fill='black')
    canvas.save(output / 'albedo.png')
    paths = [source, ROOT / 'src/render/shaders/facadeBands.glsl', ROOT / 'src/render/shaders/facadePattern.glsl',
             ROOT / 'src/render/shaders/facadeOpenings.glsl', Path(__file__)]
    report = dict(gpu_executed=True, scope=__doc__, command=command, exit=run.returncode,
                  probes=len(probes), controls=controls, channel_max_abs_error=error.max(axis=0).tolist(),
                  binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                  palette_sha256=hashlib.sha256(args.palette.read_bytes()).hexdigest(),
                  source_sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
    success.write_text(json.dumps(report, indent=2) + '\n')
    print('Native facade comparison passed:', len(probes), 'outputs; maximum error', error.max())


if __name__ == '__main__':
    main()
