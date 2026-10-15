"""Check the existing mask candidates on sheared and rank-one pixel footprints.

Uses the same area integral, heuristic fade and three recipes as the footprint
study. Rank-one cases evaluate the limiting parameter-square integral, without
dividing by a vanishing determinant. No GPU, BRDF or lit Place is evaluated.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np
from PIL import Image, ImageDraw

from ground_litter import ROOT, constituents, optical
from ground_litter_footprint import candidates, colour
from ground_litter_motion import reference


def footprints():
    cases, first, second = [], [], []
    for width in (.25, 1., 1.5):
        for aspect in (1., .125):
            for angle in (0., 45.):
                for opening in (-175., -90., -30., -5., 0., 5., 30., 90., 175., 180.):
                    theta, phi = np.deg2rad([angle, angle + opening])
                    dx = width * np.array([np.cos(theta), np.sin(theta)])
                    dy = width * aspect * np.array([np.cos(phi), np.sin(phi)])
                    determinant = dx[0] * dy[1] - dx[1] * dy[0]
                    cases.append(dict(width=width, aspect=aspect, angle_deg=angle,
                                      opening_deg=opening, determinant=float(determinant)))
                    first.append(dx)
                    second.append(dy)
    return cases, np.array(first)[:, None, :], np.array(second)[:, None, :]


def compare(points, templates, catalog):
    cases, dx, dy = footprints()
    at = np.broadcast_to(points, (len(cases),) + points.shape)
    fine = reference(at, dx, dy, 128)
    coarse = reference(at, dx, dy, 64)
    convergence = np.max(np.abs(fine - coarse), axis=-1)
    assert np.max(convergence) < .000125, float(np.max(convergence))
    records = []
    for index, case in enumerate(cases):
        fields = candidates(points, dx[index, 0], dy[index, 0])
        reversed_fields = candidates(points, dx[index, 0], -dy[index, 0])
        swapped_fields = candidates(points, dy[index, 0], dx[index, 0])
        for name, field in fields.items():
            assert np.isfinite(field).all() and np.max(np.abs(field)) <= .5 + 1e-12
            np.testing.assert_allclose(field, reversed_fields[name], rtol=0, atol=1e-12)
            np.testing.assert_allclose(field, swapped_fields[name], rtol=0, atol=1e-12)
        for template in templates:
            base, litter, coverage = constituents(template, catalog)
            delta = optical(litter, catalog['moistureModel']) - optical(base, catalog['moistureModel'])
            scale = 2 * min(coverage, 1 - coverage)
            errors = {}
            for name, field in fields.items():
                residual = scale * (field - fine[index])[..., None] * delta
                errors[name] = dict(rgb_rms=float(np.sqrt(np.mean(residual[:, :3] ** 2))),
                                    rgb_max=float(np.max(np.abs(residual[:, :3]))),
                                    roughness_max=float(np.max(np.abs(residual[:, 3]))))
            records.append(dict(**case, template=template['name'], errors=errors,
                                reference_refinement_max_field=float(convergence[index])))
    return records


def picture(output, worst, template, catalog):
    cases, dx, dy = footprints()
    index = next(i for i, row in enumerate(cases)
                 if all(row[key] == worst[key] for key in row))
    dx, dy = dx[index, 0], dy[index, 0]
    x, y = np.meshgrid(np.linspace(-2, 2, 64), np.linspace(-1.5, 1.5, 48))
    at = np.stack([x, y], axis=-1)
    fields = dict(reference=reference(at, dx, dy, 128), **candidates(at, dx, dy))
    base, litter, coverage = constituents(template, catalog)
    optics = optical(base, catalog['moistureModel']), optical(litter, catalog['moistureModel']), coverage
    canvas = Image.new('RGB', (1024, 240), 'white')
    draw = ImageDraw.Draw(canvas)
    draw.text((6, 4), f"Albedo only, {template['name']}; width={worst['width']}; aspect={worst['aspect']}; opening={worst['opening_deg']} deg", fill='black')
    for col, name in enumerate(('reference', 'bounding_box', 'gauss4', 'gauss9')):
        draw.text((col * 256 + 6, 24), name, fill='black')
        image = colour(fields[name], optics).resize((256, 192), Image.Resampling.NEAREST)
        canvas.paste(image, (col * 256, 44))
    canvas.save(output / 'shear.png')


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
    records = compare(points, templates, catalog)
    worst = max(records, key=lambda row: row['errors']['bounding_box']['rgb_max'])
    report = dict(scope=__doc__, footprints=len(records) // len(templates), points=len(points),
                  reference_side=128, refinement_side=64,
                  source_sha256={name: hashlib.sha256((Path(__file__).parent / name).read_bytes()).hexdigest()
                                 for name in ('ground_litter_shear.py', 'ground_litter_footprint.py',
                                              'ground_litter_motion.py', 'ground_litter_filter.py')},
                  input_sha256={name: hashlib.sha256((ROOT / 'src/assets/world' / name).read_bytes()).hexdigest()
                                for name in ('ground-materials.json', 'vegetation.json')},
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  results=records, worst_bounding_box=worst)
    (output / 'shear.json').write_text(json.dumps(report, indent=2) + '\n')
    picture(output, worst, next(t for t in templates if t['name'] == worst['template']), catalog)
    print('120 footprints: reference refinement, boundedness, reversal and axis-swap identities pass')


if __name__ == '__main__':
    main()
