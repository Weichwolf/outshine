"""Compare direct optical-field lookups with an octahedral source table before GGX filtering."""

import argparse
from dataclasses import replace
import json
from pathlib import Path
import time

import numpy as np

from cloud_field import Layer, transmittance


def oct_direction(uv):
    uv = uv.copy()
    for axis in [0, 1]:
        outside = (uv[..., axis] < 0.0) | (uv[..., axis] > 1.0)
        uv[..., 1-axis] = np.where(outside, 1.0-uv[..., 1-axis], uv[..., 1-axis])
        uv[..., axis] = np.where(uv[..., axis] < 0.0, -uv[..., axis], uv[..., axis])
        uv[..., axis] = np.where(uv[..., axis] > 1.0, 2.0-uv[..., axis], uv[..., axis])
    p = 2.0 * uv - 1.0
    z = 1.0 - np.sum(np.abs(p), axis=-1)
    xy = np.where((z < 0.0)[..., None], (1.0 - np.abs(p[..., ::-1])) * np.where(p >= 0.0, 1.0, -1.0), p)
    direction = np.concatenate([xy, z[..., None]], axis=-1)
    return direction / np.linalg.norm(direction, axis=-1, keepdims=True)


def oct_coordinates(direction):
    n = direction / np.sum(np.abs(direction), axis=-1, keepdims=True)
    p = np.where((n[..., 2] < 0.0)[..., None],
                 (1.0 - np.abs(n[..., 1::-1])) * np.where(n[..., :2] >= 0.0, 1.0, -1.0), n[..., :2])
    return p * 0.5 + 0.5


def bilinear(table, direction, side):
    coordinate = oct_coordinates(direction) * side + 0.5
    lower = np.floor(coordinate).astype(int)
    weight = coordinate - lower
    x, y = lower[..., 0], lower[..., 1]
    return ((1.0-weight[..., 1]) * ((1.0-weight[..., 0])*table[y, x] + weight[..., 0]*table[y, x+1])
            + weight[..., 1]*((1.0-weight[..., 0])*table[y+1, x] + weight[..., 0]*table[y+1, x+1]))


def convolution_rays(normal, roughness, diffuse):
    count = 128
    bits = np.arange(count, dtype=np.uint32)
    radical = np.zeros(count)
    for bit in range(32):
        radical += ((bits >> bit) & 1) * 2.0**(-1-bit)
    xi = (np.arange(count)+0.5)/count
    phi = 2.0*np.pi*radical
    up = np.array([0.0, 0.0, 1.0]) if abs(normal[2]) < 0.999 else np.array([1.0, 0.0, 0.0])
    tangent = np.cross(up, normal); tangent /= np.linalg.norm(tangent)
    cosine = np.sqrt(1.0-xi) if diffuse else np.sqrt((1.0-xi)/(1.0+(roughness**4-1.0)*xi))
    sine = np.sqrt(np.maximum(0.0, 1.0-cosine*cosine))
    half = (tangent*np.cos(phi)[:, None]*sine[:, None] + np.cross(normal, tangent)*np.sin(phi)[:, None]*sine[:, None]
            + normal*cosine[:, None])
    direction = half if diffuse else 2.0*np.sum(half*normal, axis=-1)[:, None]*half-normal
    weight = np.ones(count) if diffuse else np.maximum(0.0, direction@normal)
    return direction, weight


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    coordinates = np.stack(np.meshgrid((np.arange(4)+0.5)/4, (np.arange(4)+0.5)/4), axis=-1)
    normals = oct_direction(coordinates).reshape(-1, 3)
    rays, weights = zip(*(convolution_rays(normal, roughness, diffuse)
                          for normal in normals for roughness, diffuse in [(0.2, False), (0.6, False), (1.0, False), (1.0, True)]))
    rays, weights = np.array(rays), np.array(weights)
    np.testing.assert_allclose(oct_direction(oct_coordinates(rays)), rays, atol=1e-14)
    origin = np.array([0.0, 1772.0, 0.0])
    records = []
    for cover in [1.0, 0.65]:
        layer = replace(Layer(), cover=cover)
        start = time.perf_counter()
        direct = transmittance(origin, rays[..., [0, 2, 1]], layer, 16)
        direct_ms = (time.perf_counter()-start)*1000.0
        filtered = np.sum(direct*weights, axis=-1)/np.sum(weights, axis=-1)
        for side in [64, 128, 256]:
            grid = np.stack(np.meshgrid((np.arange(side+2)-0.5)/side, (np.arange(side+2)-0.5)/side), axis=-1)
            start = time.perf_counter()
            table = transmittance(origin, oct_direction(grid)[..., [0, 2, 1]], layer, 16)
            prepared_ms = (time.perf_counter()-start)*1000.0
            lookup = bilinear(table, rays, side)
            approximate = np.sum(lookup*weights, axis=-1)/np.sum(weights, axis=-1)
            error = np.abs(approximate-filtered)
            record = dict(cover=cover, side=side, normals=len(normals), filtered_cases=len(filtered),
                          mean_filtered_abs=float(error.mean()), max_filtered_abs=float(error.max()),
                          direct_python_ms=direct_ms, table_python_ms=prepared_ms,
                          full_atlas_direct_ray_queries=66*66*(1+7*128),
                          source_table_ray_queries=(side+2)**2)
            records.append(record); print(json.dumps(record))
    report = dict(records=records, scope='Identical 16-sample transmittance source, same 128 GGX/cosine rays. No cloud-scattering, receiver-depth, engine-image or GPU-time proof.')
    (args.output/'environment-comparison.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__ == '__main__':
    main()
