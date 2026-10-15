"""Compare the ground mask's bounding rectangle with the actual pixel parallelogram.

This evaluates optical parameters, not lit Places or GPU cost. Widths/coordinates
are normalized by the recipe's coarse wavelength. The reference uses the same
one-to-two-cell fade with the actual screen derivative length; it separately
reports the effect of applying the current conservative bounding-box fade.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np
from PIL import Image, ImageDraw

from ground_detail import noise2
from ground_litter import ROOT, constituents, optical
from ground_litter_filter import rectangular_field


def visibility(width):
    t = np.clip(width - 1, 0, 1)
    return 1 - t * t * (3 - 2 * t)


def derivatives(width, aspect, angle):
    radians = np.deg2rad(angle)
    dx = width * np.array([np.cos(radians), np.sin(radians)])
    dy = width * aspect * np.array([-np.sin(radians), np.cos(radians)])
    return dx, dy


def sampled(at, dx, dy, nodes, weights):
    total = np.zeros(at.shape[:-1])
    for y, wy in zip(nodes, weights):
        for x, wx in zip(nodes, weights):
            total += wx * wy * (noise2(at + x * dx + y * dy) - .5)
    return total * visibility(max(np.linalg.norm(dx), np.linalg.norm(dy)))


def midpoint(at, dx, dy, count):
    nodes = (np.arange(count) + .5) / count - .5
    return sampled(at, dx, dy, nodes, np.full(count, 1 / count))


def gauss(at, dx, dy, count):
    nodes, weights = np.polynomial.legendre.leggauss(count)
    return sampled(at, dx, dy, nodes * .5, weights * .5)


def candidates(at, dx, dy):
    bounds = np.abs(dx) + np.abs(dy)
    box = rectangular_field(at, bounds, 1.)
    fade = visibility(max(np.linalg.norm(dx), np.linalg.norm(dy)))
    return dict(bounding_box=box, point=(noise2(at) - .5) * fade,
                gauss4=gauss(at, dx, dy, 2), gauss9=gauss(at, dx, dy, 3), mean=np.zeros(at.shape[:-1]))


def colour(field, optics):
    first, second, coverage = optics
    weight = coverage + 2 * min(coverage, 1 - coverage) * field
    linear = first[:3] + (second[:3] - first[:3]) * weight[..., None]
    srgb = np.where(linear <= .0031308, 12.92 * linear, 1.055 * linear ** (1 / 2.4) - .055)
    return Image.fromarray(np.uint8(np.clip(srgb, 0, 1) * 255))


def comparison(points, templates, catalog):
    records = []
    convergence = 0.
    for width in (.05, .25, .5, .8, 1., 1.5):
        for aspect in (1., .125, .01):
            for angle in (0., 15., 30., 45., 60., 75., 90.):
                dx, dy = derivatives(width, aspect, angle)
                reference = midpoint(points, dx, dy, 64)
                coarse = midpoint(points, dx, dy, 32)
                convergence = max(convergence, float(np.max(np.abs(coarse - reference))))
                actual = candidates(points, dx, dy)
                bbox_fade = visibility(np.max(np.abs(dx) + np.abs(dy)))
                true_fade = visibility(width)
                for template in templates:
                    base, litter, coverage = constituents(template, catalog)
                    delta = optical(litter, catalog['moistureModel']) - optical(base, catalog['moistureModel'])
                    amplitude = 2 * min(coverage, 1 - coverage)
                    errors = {}
                    for name, field in actual.items():
                        residual = amplitude * (field - reference)[..., None] * delta
                        errors[name] = dict(rgb_rms=float(np.sqrt(np.mean(residual[:, :3] ** 2))),
                                            rgb_max=float(np.max(np.abs(residual[:, :3]))),
                                            roughness_max=float(np.max(np.abs(residual[:, 3]))))
                    records.append(dict(template=template['name'], width=width, aspect=aspect,
                                        angle_deg=angle, bounding_fade=float(bbox_fade),
                                        derivative_fade=float(true_fade), errors=errors))
    return records, convergence


def picture(output, worst, template, catalog):
    base, litter, coverage = constituents(template, catalog)
    optics = optical(base, catalog['moistureModel']), optical(litter, catalog['moistureModel']), coverage
    x, y = np.meshgrid(np.linspace(-2, 2, 128), np.linspace(-1.5, 1.5, 96))
    at = np.stack([x, y], axis=-1)
    dx, dy = derivatives(worst['width'], worst['aspect'], worst['angle_deg'])
    fields = dict(reference=midpoint(at, dx, dy, 64), **candidates(at, dx, dy))
    canvas = Image.new('RGB', (768, 350), 'white')
    draw = ImageDraw.Draw(canvas)
    draw.text((4, 3), f"Albedo only: {template['name']}; width={worst['width']}; aspect={worst['aspect']}; angle={worst['angle_deg']}", fill='black')
    for index, name in enumerate(('reference', 'bounding_box', 'gauss4', 'gauss9', 'point', 'mean')):
        col, row = index % 3, index // 3
        draw.text((col * 256 + 4, row * 164 + 24), name, fill='black')
        canvas.paste(colour(fields[name], optics).resize((256, 144)), (col * 256, row * 164 + 40))
    canvas.save(output / 'pixel-footprint.png')


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
    points = np.random.default_rng(196).uniform(-8, 8, (128, 2))
    records, convergence = comparison(points, templates, catalog)
    assert convergence < .0005, convergence
    worst = max(records, key=lambda r: r['errors']['bounding_box']['rgb_max'])
    report = dict(scope=__doc__,
                  commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  input_sha256={name: hashlib.sha256((ROOT / 'src/assets/world' / name).read_bytes()).hexdigest()
                                for name in ('ground-materials.json', 'vegetation.json')},
                  method_hash_upper_bounds=dict(point=4, bounding_box=16, gauss4=16, gauss9=36, mean=0),
                  reference_points=128, reference_side=64, refinement_max_field_delta=convergence,
                  implementation_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  worst_bounding_box_case=worst, results=records)
    (output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    template = next(r for r in templates if r['name'] == worst['template'])
    picture(output, worst, template, catalog)
    print(json.dumps(dict(refinement_max_field_delta=convergence, worst_bounding_box_case=worst), indent=2))


if __name__ == '__main__':
    main()
