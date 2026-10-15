"""Compare coating filters and their covariance with real native facade openings."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import time

import numpy as np

from outshine_ref.buildings.facades import rectangle_coverage
from rock_direction import noise

ROOT = Path(__file__).resolve().parents[3]
SCALE_M = 1.5
CONTRAST = .16
WALL = np.array([.74, .71, .65])


def controls():
    rng = np.random.default_rng(278)
    positions = rng.uniform(-8, 8, (16, 2))
    values = []
    for angle in (0., .7, 1.3):
        u, v = np.array([np.cos(angle), np.sin(angle)]), np.array([-np.sin(angle), np.cos(angle)])
        for width in (.025, .1, .25, .5, .8):
            for shear in (0., .8, 1.8):
                for at in positions:
                    values.append((at, width * SCALE_M * u, width * SCALE_M * (v + shear * u)))
    return [np.array([item[index] for item in values]) for index in range(3)]


def coating(points):
    world = np.stack([points[..., 0] * np.cos(.43), points[..., 1], points[..., 0] * np.sin(.43)], axis=-1)
    return noise((world + [.31, 2.73, -4.27]) / SCALE_M) - .5


def opaque(points, profile):
    phase = np.remainder(points / profile[:2], 1)
    return ~np.all(np.abs(phase - .5) < profile[2:] / 2, axis=-1)


def sample(at, dx, dy, profile, count, joint=False, midpoint=False):
    nodes = (np.arange(count) + .5) / count - .5 if midpoint else np.polynomial.legendre.leggauss(count)[0] / 2
    weights = np.full(count, 1 / count) if midpoint else np.polynomial.legendre.leggauss(count)[1] / 2
    result = np.zeros(len(at))
    for y, wy in zip(nodes, weights, strict=True):
        for x, wx in zip(nodes, weights, strict=True):
            points = at + x * dx + y * dy
            result += wx * wy * coating(points) * (opaque(points, profile) if joint else 1)
    return result


def partitioned(at, dx, dy, profile, count):
    result = np.zeros(len(at))
    width = (np.abs(dx) + np.abs(dy)) / count / profile[:2]
    for y in (np.arange(count) + .5) / count - .5:
        for x in (np.arange(count) + .5) / count - .5:
            points = at + x * dx + y * dy
            wall = 1 - rectangle_coverage(points / profile[:2], width, profile[2:])
            result += coating(points) * wall / (count * count)
    return result


def area_reference(at, dx, dy, profile, count, separate=False):
    nodes = (np.arange(count) + .5) / count - .5
    x, y = np.meshgrid(nodes, nodes)
    result, field_means, wall_means = [], [], []
    for start in range(0, len(at), 16):
        points = at[start:start + 16, None] + x.ravel()[None, :, None] * dx[start:start + 16, None]
        points += y.ravel()[None, :, None] * dy[start:start + 16, None]
        field, wall = coating(points), opaque(points, profile)
        result.extend(np.mean(field * wall, axis=1))
        if separate:
            field_means.extend(np.mean(field, axis=1))
            wall_means.extend(np.mean(wall, axis=1))
    return (np.array(result), np.array(field_means), np.array(wall_means)) if separate else np.array(result)


def plot(report, output):
    import matplotlib
    matplotlib.use('Agg')
    from matplotlib import pyplot as plt
    figure, axes = plt.subplots(1, 2, figsize=(10, 4), sharey=True, constrained_layout=True)
    for ax, profile in zip(axes, report['profiles'], strict=True):
        for kind, label in [('product', 'filtered field x box coverage'), ('joint', 'joint point samples'),
                            ('partitioned', 'filtered subcells')]:
            rows = [r for r in profile['filters'] if
                    ('partitioned' if r.get('partitioned') else 'joint' if r['joint'] else 'product') == kind]
            ax.loglog([r['noise_corner_hashes'] for r in rows],
                      [r['selected_refined_max_rgb_error'] for r in rows], 'o-', label=label)
        ax.set_title(f"Native facade style {profile['style']}")
        ax.set_xlabel('Requested noise corner hashes; no GPU time')
        ax.grid(True, which='both', alpha=.25)
    axes[0].set_ylabel('Largest selected linear RGB error, 256x256 reference')
    axes[0].legend(fontsize=8)
    figure.suptitle('Coating / opening covariance - CPU counterexamples, no native image proof')
    figure.savefig(output / 'comparison.png', dpi=160)
    figure.savefig(output / 'comparison.svg')
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--plot', action='store_true')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    at, dx, dy = controls()
    paths = [ROOT / 'src/content/shade/FacadeProfiles.inc', ROOT / 'src/generators/building/BuildingMaterials.h']
    profiles = [list(map(float, line.rstrip(')').split(',')[-4:])) for line in paths[0].read_text().splitlines()]
    native_wall = np.array(list(map(float, re.search(r'Colour\{\{([^}]+)', paths[1].read_text())[1].replace('f', '').split(','))))
    np.testing.assert_array_equal(native_wall, WALL)
    report = dict(scope=__doc__, scale_m=SCALE_M, contrast=CONTRAST, cases_per_profile=len(at), profiles=[])
    for style in (1, 8):
        profile = np.array(profiles[style])
        box_wall = 1 - rectangle_coverage(at / profile[:2], (np.abs(dx) + np.abs(dy)) / profile[:2], profile[2:])
        begin = time.perf_counter()
        ref32 = area_reference(at, dx, dy, profile, 32)
        ref64 = area_reference(at, dx, dy, profile, 64)
        ref128, field128, wall128 = area_reference(at, dx, dy, profile, 128, separate=True)
        rows, candidates = [], []
        for count in (1, 2, 3):
            for joint in (False, True):
                start = time.perf_counter()
                actual = sample(at, dx, dy, profile, count, joint)
                if not joint:
                    actual *= box_wall
                candidates.append(actual)
                error = np.max(np.abs(actual - ref128)) * CONTRAST * native_wall.max()
                rows.append(dict(points=count * count, joint=joint, max_linear_rgb_error=float(error),
                                 elapsed_s=time.perf_counter() - start, noise_corner_hashes=8 * count * count))
        for count in (2, 3, 4):
            start = time.perf_counter()
            actual = partitioned(at, dx, dy, profile, count)
            candidates.append(actual)
            rows.append(dict(points=count * count, partitioned=True,
                max_linear_rgb_error=float(np.max(np.abs(actual - ref128)) * CONTRAST * native_wall.max()),
                elapsed_s=time.perf_counter() - start, noise_corner_hashes=8 * count * count))
        assert np.isfinite(candidates).all() and np.max(np.abs(candidates)) <= .5
        np.testing.assert_allclose(area_reference(at[:8], dx[:8], dy[:8], profile, 32),
                                   area_reference(at[:8], -dy[:8], dx[:8], profile, 32),
                                   atol=1e-13, rtol=1e-13)
        worst = np.unique(np.concatenate([np.argsort(np.abs(candidate - ref128))[-8:] for candidate in candidates]))
        selected128 = ref128[worst]
        ref256 = area_reference(at[worst], dx[worst], dy[worst], profile, 256)
        for row, candidate in zip(rows, candidates, strict=True):
            row['selected_refined_max_rgb_error'] = float(np.max(np.abs(candidate[worst] - ref256)) * CONTRAST * native_wall.max())
        np.savez_compressed(args.output / f'style-{style}.npz', at=at, dx=dx, dy=dy,
                            ref32=ref32, ref64=ref64, ref128=ref128, field128=field128, wall128=wall128, selected=worst, ref256=ref256,
                            candidates=np.array(candidates))
        covariance = (ref128 - field128 * wall128) * CONTRAST * native_wall.max()
        aligned = (np.abs(dx[:, 1]) < 1e-12) & (np.abs(dy[:, 0]) < 1e-12)
        report['profiles'].append(dict(style=style, filters=rows,
            same_reference_covariance_max_rgb_bias=float(np.max(np.abs(covariance))),
            aligned_same_reference_covariance_max_rgb_bias=float(np.max(np.abs(covariance[aligned]))),
            window_box_max_area_difference=float(np.max(np.abs(box_wall - wall128))),
            reference_32_64_max_rgb_change=float(np.max(np.abs(ref64 - ref32)) * CONTRAST * native_wall.max()),
            reference_64_128_max_rgb_change=float(np.max(np.abs(ref128 - ref64)) * CONTRAST * native_wall.max()),
            selected_128_256_max_rgb_change=float(np.max(np.abs(ref256 - selected128)) * CONTRAST * native_wall.max()),
            selected_indices=worst.tolist(), elapsed_s=time.perf_counter() - begin))
        print(style, rows, flush=True)
    report['source_sha256'] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths + [Path(__file__), ROOT / 'test/experiments/render/rock_direction.py',
                                                                                 ROOT / 'lib/src/outshine_ref/buildings/facades.py']}
    (args.output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    if args.plot:
        plot(report, args.output)


if __name__ == '__main__':
    main()
