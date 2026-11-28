"""Replay shadow cascade overlap on resident native geometry bounds."""

import argparse
import json
from pathlib import Path

import numpy as np
from cluster_chunk_demand import affine_bounds, rows


def atlas_window(radius, projected_eye, side):
    metres_per_texel = 2 * radius / side
    return np.floor(projected_eye / metres_per_texel) * metres_per_texel


def unread_half_extent(fine_radius, coarse_radius, side):
    return 0.8 * fine_radius - 2 * (2 * coarse_radius / side)


def verify_filter_support():
    rng = np.random.default_rng(1958)
    side = 2048
    for fine, coarse in ((256, 1024), (1024, 4096), (4096, 10250)):
        for eye in rng.uniform(-10000, 10000, size=(16, 2)):
            fine_centre = atlas_window(fine, eye, side)
            coarse_centre = atlas_window(coarse, eye, side)
            guard = unread_half_extent(fine, coarse, side)
            receivers = fine_centre + rng.uniform(-1.1, 1.1, size=(8192, 2)) * fine
            samples_coarse = (np.abs(receivers - fine_centre) > 0.8 * fine).any(axis=1)
            receivers = receivers[samples_coarse]
            coarse_pixel = (receivers - coarse_centre + coarse) * (side / (2 * coarse))
            base = np.floor(coarse_pixel - 0.5)
            for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
                pixel = np.clip(base + (x, y), 0, side - 1)
                world = coarse_centre + (pixel + 0.5) * (2 * coarse / side) - coarse
                assert not (np.abs(world - fine_centre) < guard).all(axis=1).any()


def verify_exposed_texels():
    rng = np.random.default_rng(1975)
    side = 32
    all_texels = np.indices((96, 96)).transpose(1, 2, 0)

    def rect(lo, hi):
        return ((all_texels >= lo) & (all_texels < hi)).all(axis=2)

    previous, old_hole = rect((32, 32), (64, 64)), rect((40, 40), (56, 56))
    for _ in range(128):
        start = rng.integers(12, 52, size=2)
        current = rect(start, start + side)
        half = rng.integers(0, 15)
        hole = rect(start + 16 - half, start + 16 + half)
        dirty = (current & ~previous) | (old_hole & current & ~hole)
        written = (previous & ~old_hole) | (dirty & ~hole)
        assert not (current & ~hole & ~written).any()
        previous, old_hole = current, hole


def geometry(snapshot):
    roots = rows(snapshot, "roots", 12, np.uint32)
    batches = rows(snapshot, "batches", 12, np.uint32)
    templates = rows(snapshot, "templates", 4, np.uint32)
    spheres = rows(snapshot, "cluster_spheres", 12, np.float32)
    poses = (
        rows(snapshot, "placements", 16, np.float32)
        .reshape(-1, 4, 4)
        .transpose(0, 2, 1)
    )
    owners = np.repeat(np.arange(len(roots)), roots[:, 5])
    addresses = roots[owners, 4] + np.arange(len(owners)) - roots[owners, 6]
    valid = ((roots[owners, 11] & 3) == 1) & (batches[roots[owners, 7], 4] != 1)
    items = templates[addresses[valid]]
    xyz, radius = affine_bounds(
        poses[roots[owners[valid], 8]].astype(float), spheres[items[:, 0], :4]
    )
    return xyz, radius, items[:, 2].astype(np.int64) // 3


def measure(snapshot):
    xyz, radius, triangles = geometry(snapshot)
    info = json.loads((snapshot / "geometry.json").read_text())
    report = json.loads((snapshot / "benchmark.json").read_text())
    metrics = {m["name"]: m["value"] for m in report["metrics"]}
    eye = np.asarray(info["eye_from_anchor_m"])
    elevation = np.radians(metrics["the sun stands this high"])
    stand = metrics["the shadow radius it stood on"]
    side = 2048
    outer = np.ceil((stand + np.ceil(np.linalg.norm(eye) / 256) * 256) / 256)
    outer *= 256 * (1 + 2 / side)
    samples = []
    for bearing in range(0, 360, 45):
        az = np.radians(bearing)
        light = -np.array(
            [
                np.sin(az) * np.cos(elevation),
                np.sin(elevation),
                -np.cos(az) * np.cos(elevation),
            ]
        )
        right = np.cross([0, 1, 0], light)
        right /= np.linalg.norm(right)
        axes = np.stack((right, np.cross(light, right)), axis=1)
        projected = xyz @ axes
        receiver = eye @ axes
        depth = np.abs(xyz @ light) <= stand * 2 + radius
        estimates = []
        for fine, coarse in zip((256, 1024, 4096), (1024, 4096, outer), strict=True):
            target = atlas_window(coarse, receiver, side)
            source = atlas_window(fine, receiver, side)
            maybe = ((np.abs(projected - target) - radius[:, None]) <= coarse).all(
                axis=1
            ) & depth
            guard = unread_half_extent(fine, coarse, side)
            hidden = (np.abs(projected - source) + radius[:, None] < guard).all(axis=1)
            estimates.append(
                [int(triangles[maybe].sum()), int(triangles[maybe & hidden].sum())]
            )
        samples.append(estimates)
    work = np.asarray(samples)
    return {
        "casters": len(triangles),
        "coarse_radii_m": [1024, 4096, outer],
        "average_candidate_triangles": work[..., 0].mean(axis=0).astype(int).tolist(),
        "average_unread_triangles": work[..., 1].mean(axis=0).astype(int).tolist(),
        "unread_min_max": np.stack(
            (work[..., 1].min(axis=0), work[..., 1].max(axis=0))
        ).tolist(),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path)
    args = parser.parse_args()
    verify_filter_support()
    verify_exposed_texels()
    print(json.dumps(measure(args.snapshot)))
