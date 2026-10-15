"""Compare actual Vulkan RGB/T source tables with direct rays and GGX/cosine probes."""

import argparse
import json
from pathlib import Path

import numpy as np

from cloud_comparison import view_rays
from cloud_environment import bilinear, bilinear_uv, convolution_rays, oct_direction
from cloud_field import Layer
from cloud_radiance import background, save_picture


def read_rgba(path, shape):
    value = np.fromfile(path, dtype=np.float32).reshape(*shape, 4)
    assert np.isfinite(value).all() and np.all(value >= 0.0)
    assert np.all(value[..., 3] <= 1.0)
    return value


def errors(value, reference):
    delta = np.abs(value-reference)
    return dict(p99_rgb_abs=float(np.quantile(delta[..., :3], 0.99)),
                max_rgb_abs=float(delta[..., :3].max()),
                p99_t_abs=float(np.quantile(delta[..., 3], 0.99)),
                max_t_abs=float(delta[..., 3].max()))


def filter_probes():
    uv = np.stack(np.meshgrid((np.arange(4)+0.5)/4, (np.arange(4)+0.5)/4), axis=-1)
    normals = oct_direction(uv).reshape(-1, 3)
    rays, weights = zip(*(convolution_rays(normal, roughness, diffuse)
                          for normal in normals
                          for roughness, diffuse in [(0.2, False), (0.6, False),
                                                      (1.0, False), (1.0, True)]))
    return np.array(rays), np.array(weights)


def filtered(values, weights):
    return np.sum(values*weights[..., None], axis=1)/np.sum(weights, axis=1)[..., None]


def horizon_coordinates(direction, height_px):
    radius, height = 6360000.0, 1772.0
    horizon = np.arccos(-np.sqrt(height*(2.0*radius+height))/(radius+height))
    zenith = np.arccos(np.clip(direction[..., 2], -1.0, 1.0))
    v = np.where(zenith < horizon,
                 (1.0-np.sqrt(np.maximum(0.0, 1.0-zenith/horizon)))*0.5,
                 np.sqrt(np.maximum(0.0, (zenith-horizon)/(np.pi-horizon)))*0.5+0.5)
    v = np.where(v < 0.5, np.minimum(v, 0.5-0.5/height_px),
                 np.maximum(v, 0.5+0.5/height_px))
    u = np.mod(np.arctan2(direction[..., 1], direction[..., 0])/(2.0*np.pi), 1.0)
    return np.stack([u, v], axis=-1)


def compare(label, table, sample, directions, sky, sky_mask, reference, rays, weights,
            direct_filtered, output):
    records = []
    for precision in ['float32', 'float16']:
        values = table if precision == 'float32' else table.astype(np.float16).astype(np.float32)
        cloud = sample(values, directions[..., [0, 2, 1]])
        cloud[~sky_mask] = [0.0, 0.0, 0.0, 1.0]
        picture = np.concatenate([cloud[..., :3]+cloud[..., 3:4]*sky, cloud[..., 3:4]], axis=-1)
        lookup = sample(values, rays)
        record = dict(mapping=label, precision=precision,
                      bytes=int(values.size*(2 if precision == 'float16' else 4)),
                      camera_sky=errors(picture[sky_mask], reference[sky_mask]),
                      filtered=errors(filtered(lookup, weights), direct_filtered),
                      quantization=errors(values, table))
        records.append(record)
        print(json.dumps(record))
        if precision == 'float16':
            save_picture(picture, output/f'{label}.png')
    return records


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--camera-reference', required=True, type=Path)
    parser.add_argument('--sides', type=int, nargs='+', default=[64, 128, 256, 512])
    parser.add_argument('--horizon-sizes', nargs='*', default=[])
    parser.add_argument('--width', type=int, default=1280)
    parser.add_argument('--height', type=int, default=720)
    args = parser.parse_args()
    if not (1 <= args.width <= 4096 and 1 <= args.height <= 4096 and
            all(2 <= side <= 2048 for side in args.sides)):
        parser.error('require dimensions in [1,4096] and source sides in [2,2048]')
    args.output.mkdir(parents=True, exist_ok=True)
    clear = read_rgba(args.input/'source-clear.f32', (4, 4))
    np.testing.assert_array_equal(clear[..., :3], 0.0)
    np.testing.assert_array_equal(clear[..., 3], 1.0)
    reference = read_rgba(args.camera_reference, (args.height, args.width))
    directions = view_rays(args.width, args.height, 240.0, -0.5, 33.97)
    sky, maximum = background(np.array([0.0, 1772.0, 0.0]), directions, Layer(cover=0.65))
    sky_mask = ~np.isfinite(maximum)
    rays, weights = filter_probes()
    direct = read_rgba(args.input/'filter-reference.f32', (64, 128))
    direct_filtered = filtered(direct, weights)
    records = []
    for side in args.sides:
        table = read_rgba(args.input/f'source-{side}.f32', (side+2, side+2))
        sample = lambda values, ray, side=side: bilinear(values, ray, side)
        records += compare(f'oct-{side}', table, sample, directions, sky, sky_mask,
                           reference, rays, weights, direct_filtered, args.output)
        border_error = max(np.max(np.abs(table[:, 0]-table[::-1, 1])),
                           np.max(np.abs(table[:, -1]-table[::-1, -2])),
                           np.max(np.abs(table[0]-table[1, ::-1])),
                           np.max(np.abs(table[-1]-table[-2, ::-1])))
        for row in records[-2:]:
            row['border_max_abs'] = float(border_error)
    for size in args.horizon_sizes:
        width, height = map(int, size.split('x'))
        if not (2 <= width <= 2048 and 2 <= height <= 2048):
            parser.error('require horizon source dimensions in [2,2048]')
        table = read_rgba(args.input/f'horizon-{size}.f32', (height+2, width+2))
        extent = np.array([width, height])
        sample = lambda values, ray, extent=extent: bilinear_uv(
            values, horizon_coordinates(ray, extent[1]), extent)
        records += compare(f'horizon-{size}', table, sample, directions, sky, sky_mask,
                           reference, rays, weights, direct_filtered, args.output)
        border_error = max(np.max(np.abs(table[:, 0]-table[:, -2])),
                           np.max(np.abs(table[:, -1]-table[:, 1])))
        for row in records[-2:]:
            row['border_max_abs'] = float(border_error)
    report = dict(records=records,
                  clear_identity_exact=True,
                  scope='Same pure-cloud RGB/T kernel and declared toy light. No clear-air, '
                        'ground-irradiance feedback, receiver-depth, client image, motion or GPU-time proof.')
    (args.output/'source-comparison.json').write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
