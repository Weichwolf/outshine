"""Compare mask error during motion against the actual pixel-area integral.

This is an optical-parameter experiment, without lighting, TAA or a GPU backend.
The three tracks use normalized recipe wavelengths, 91 steps and 32 fixed seeds.
Frame-to-frame residuals measure erroneous changes, not all temporal variation:
the reference itself changes as a footprint rotates, moves or grows.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time

import numpy as np
from PIL import Image, ImageDraw

from ground_detail import noise2
from ground_litter import ROOT, constituents, optical
from ground_litter_footprint import candidates, colour, derivatives, visibility


def tracks(points):
    steps = np.linspace(0, 1, 91)
    for name in ('rotation', 'translation', 'growth'):
        widths = .25 + 2 * steps if name == 'growth' else np.ones_like(steps)
        angles = 90 * steps if name == 'rotation' else np.full_like(steps, 45)
        at = np.broadcast_to(points, (len(steps),) + points.shape).copy()
        if name == 'translation':
            at += steps[:, None, None] * np.array([1.4, -.9])
        dx, dy = zip(*(derivatives(w, 1., a) for w, a in zip(widths, angles)))
        yield name, at, np.array(dx)[:, None, :], np.array(dy)[:, None, :], widths, angles


def reference(at, dx, dy, side):
    nodes = (np.arange(side) + .5) / side - .5
    total = np.zeros(at.shape[:-1])
    for y in nodes:
        for x in nodes:
            total += noise2(at + x * dx + y * dy) - .5
    width = np.maximum(np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1))
    return total * visibility(width) / (side * side)


def statistics(residual):
    step = np.diff(residual, axis=0)
    return dict(rgb_rms=float(np.sqrt(np.mean(residual ** 2))),
                rgb_max=float(np.max(np.abs(residual))),
                step_rgb_rms=float(np.sqrt(np.mean(step ** 2))),
                step_rgb_max=float(np.max(np.abs(step))))


def measure(points, templates, catalog):
    records, plot_data = [], {}
    for name, at, dx, dy, widths, angles in tracks(points):
        fine = reference(at, dx, dy, 128)
        coarse = reference(at, dx, dy, 64)
        refinement = float(np.max(np.abs(fine - coarse)))
        refinement_step = np.diff(fine - coarse, axis=0)
        assert refinement < .000125, (name, refinement)
        fields = {key: [] for key in candidates(at[0], dx[0, 0], dy[0, 0])}
        for index in range(len(at)):
            current = candidates(at[index], dx[index, 0], dy[index, 0])
            for key, value in current.items():
                fields[key].append(value)
        fields = {key: np.array(value) for key, value in fields.items()}
        for field in fields.values():
            assert np.all(np.isfinite(field)) and np.max(np.abs(field)) <= .5 + 1e-12
            assert np.all(field[widths >= 2] == 0), name
        if name == 'rotation':
            assert np.max(np.abs(fine[0] - fine[-1])) < 1e-12
            for field in fields.values():
                assert np.max(np.abs(field[0] - field[-1])) < 1e-12
        for template in templates:
            base, litter, coverage = constituents(template, catalog)
            delta = optical(litter, catalog['moistureModel']) - optical(base, catalog['moistureModel'])
            amplitude = 2 * min(coverage, 1 - coverage)
            rgb_scale = amplitude * delta[:3]
            errors = {key: statistics((value - fine)[..., None] * rgb_scale)
                      for key, value in fields.items()}
            reference_steps = np.diff(fine, axis=0)[..., None] * rgb_scale
            records.append(dict(track=name, template=template['name'], frames=len(at),
                                refinement_max_field_delta=refinement,
                                refinement_step_rgb_rms=float(np.sqrt(np.mean(
                                    (refinement_step[..., None] * rgb_scale) ** 2))),
                                reference_step_rgb_rms=float(np.sqrt(np.mean(reference_steps ** 2))),
                                errors=errors))
        plot_data[name] = dict(reference=fine, **fields)
        print(name, 'reference_delta', refinement, flush=True)
    return records, plot_data


def picture(output, data, template, catalog):
    base, litter, coverage = constituents(template, catalog)
    optics = optical(base, catalog['moistureModel']), optical(litter, catalog['moistureModel']), coverage
    canvas = Image.new('RGB', (960, 750), 'white')
    draw = ImageDraw.Draw(canvas)
    draw.text((8, 6), 'Albedo motion diagnostic: alpine_grassland; all 91 steps x 32 fixed points', fill='black')
    draw.text((8, 24), 'Each column is a step; each row is one tracked point. No lighting / TAA / GPU.', fill='black')
    for row, (track, fields) in enumerate(data.items()):
        draw.text((8, 52 + row * 230), track, fill='black')
        for col, name in enumerate(('reference', 'bounding_box', 'gauss4', 'gauss9')):
            x, y = col * 240, 72 + row * 230
            draw.text((x + 6, y), name, fill='black')
            field = np.swapaxes(fields[name], 0, 1)
            image = colour(field, optics).resize((228, 192), Image.Resampling.NEAREST)
            canvas.paste(image, (x + 6, y + 18))
    canvas.save(output / 'motion.png')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error('use a directory in system temp')
    output.mkdir(parents=True, exist_ok=True)
    catalog = json.loads((ROOT / 'src/assets/world/ground-materials.json').read_text())
    recipes = json.loads((ROOT / 'src/assets/world/vegetation.json').read_text())['templates']
    templates = [r for r in recipes if r['name'] in ('mixed_broadleaf', 'conifer_forest', 'alpine_grassland')]
    points = np.random.default_rng(196).uniform(-8, 8, (32, 2))
    started = time.perf_counter()
    records, data = measure(points, templates, catalog)
    report = dict(scope=__doc__,
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  source_sha256={name: hashlib.sha256((Path(__file__).parent / name).read_bytes()).hexdigest()
                                 for name in ('ground_litter_motion.py', 'ground_litter_footprint.py',
                                              'ground_litter_filter.py', 'ground_detail.py')},
                  input_sha256={name: hashlib.sha256((ROOT / 'src/assets/world' / name).read_bytes()).hexdigest()
                                for name in ('ground-materials.json', 'vegetation.json')},
                  elapsed_experiment_seconds=time.perf_counter() - started,
                  results=records)
    (output / 'motion.json').write_text(json.dumps(report, indent=2) + '\n')
    picture(output, data, next(r for r in templates if r['name'] == 'alpine_grassland'), catalog)
    print('verified bounds, far mean, quarter-turn identity and 64/128 reference convergence')


if __name__ == '__main__':
    main()
