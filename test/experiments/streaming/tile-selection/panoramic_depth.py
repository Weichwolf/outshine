"""Replay previous-frame occlusion for a fixed-eye turn using native depth."""

import argparse
import json
from pathlib import Path
from time import perf_counter

import numpy as np

from cluster_chunk_demand import affine_bounds, rows


def frame(directory):
    metadata = json.loads((directory / "geometry.json").read_text())
    shape = tuple(int(x) for x in metadata["viewport_px"])[::-1]
    samples = rows(directory, "depth", 1, "<f4")
    if len(samples) != np.prod(shape):
        raise ValueError("snapshot does not contain depth at the reported viewport size")
    depth = samples.reshape(shape)
    clip = np.asarray(metadata["depth_clip_from_eye"]).reshape(4, 4).T
    if not np.isfinite(depth).all() or np.any((depth < 0) | (depth > 1)):
        raise ValueError("expected finite reverse-Z scene depth")
    return metadata, clip, depth


def selected(directory, eye):
    roots = rows(directory, "roots", 12, "<u4")
    templates = rows(directory, "templates", 4, "<u4")
    spheres = rows(directory, "cluster_spheres", 12, "<f4")
    models = rows(directory, "placements", 16, "<f4").reshape(-1, 4, 4).transpose(0, 2, 1)
    kept = rows(directory, "kept", 1, "<u4").ravel()
    visibility = rows(directory, "visible", 1, "<u4").ravel()
    root = np.repeat(np.arange(len(roots)), roots[:, 5])
    ordinal = np.arange(len(root)) - roots[root, 6]
    leaves = templates[roots[root, 4] + ordinal]
    index_count = np.where(visibility[root] == 2, leaves[:, 2], kept)
    have = (visibility[root] != 0) & (index_count != 0)
    placement = models[roots[root[have], 8]]
    centres, radii = affine_bounds(placement, spheres[leaves[have, 0], :4])
    active_roots = np.flatnonzero((visibility != 0) & ((roots[:, 11] & 1) != 0))
    bounds = roots[active_roots, :4].view("<f4")
    roots_centre, roots_radius = affine_bounds(models[roots[active_roots, 8]], bounds)
    return (
        centres - eye,
        radii,
        index_count[have] // 3,
        root[have],
        visibility,
        (roots_centre - eye, roots_radius, active_roots),
    )


def projected(clip, centre, radius):
    projected = np.einsum("ij,kj->ki", clip[:, :3], centre) + clip[:, 3]
    extent = radius[:, None] * np.linalg.norm(clip[:, :3], axis=1)
    low, high = projected - extent, projected + extent
    safe = low[:, 3] > 1.0e-6
    near = np.where(safe, low[:, 3], 1.0)[:, None]
    far = np.where(safe, high[:, 3], 1.0)[:, None]
    pairs = (low[:, :3] / near, low[:, :3] / far, high[:, :3] / near, high[:, :3] / far)
    lo, hi = np.minimum.reduce(pairs), np.maximum.reduce(pairs)
    return (
        np.column_stack((0.5 + lo[:, 0] * 0.5, 0.5 - hi[:, 1] * 0.5)),
        np.column_stack((0.5 + hi[:, 0] * 0.5, 0.5 - lo[:, 1] * 0.5)),
        hi[:, 2],
        safe,
    )


def min_depth_levels(depth):
    for _ in range(4):
        height, width = depth.shape
        depth = np.pad(depth, ((0, height % 2), (0, width % 2)), constant_values=1)
        depth = depth.reshape((height + 1) // 2, 2, (width + 1) // 2, 2).min(axis=(1, 3))
        yield depth


def occluded(clip, depth, centre, radius, guard_px, bias, pyramid=True):
    scale = np.array(depth.shape[::-1])
    low, high, nearest, safe = projected(clip, centre, radius)
    low = low * scale - guard_px
    high = high * scale + guard_px
    inside = (low >= 0).all(axis=1) & (high < scale).all(axis=1)
    eligible = safe & inside & (nearest > 0) & (nearest < 1)
    hidden = np.zeros(len(centre), dtype=bool)
    if pyramid:
        levels = tuple(min_depth_levels(depth))
        lod = np.searchsorted([4, 8, 16], (high - low).max(axis=1))
        for level, image in enumerate(levels):
            choose = np.flatnonzero(eligible & (lod == level))
            sample = 2 << level
            left, top = (low[choose] / sample).astype(int).T
            right, bottom = (high[choose] / sample).astype(int).T
            nearby = (right - left < 4) & (bottom - top < 4)
            # A clamped repeated read has no effect on the minimum.
            offsets = np.arange(4)
            x = np.minimum(left[nearby, None] + offsets, right[nearby, None])
            y = np.minimum(top[nearby, None] + offsets, bottom[nearby, None])
            minimum = image[y[:, :, None], x[:, None, :]].min(axis=(1, 2))
            indices = choose[nearby]
            hidden[indices] = nearest[indices] + bias < minimum
        return hidden

    for idx in np.flatnonzero(eligible):
        left, top = low[idx].astype(int)
        right, bottom = high[idx].astype(int)
        hidden[idx] = nearest[idx] + bias < depth[top : bottom + 1, left : right + 1].min()
    return hidden


def replay(old, current, guard_px, bias):
    old_meta, old_clip, old_depth = frame(old)
    new_meta, new_clip, new_depth = frame(current)
    old_camera, new_camera = [
        json.loads((directory / "benchmark.json").read_text())["motion"]["finish"]
        for directory in (old, current)
    ]
    if (
        old_meta["viewport_px"] != new_meta["viewport_px"]
        or old_camera["position_m"] != new_camera["position_m"]
        or old_camera["perspective_fov_deg"] != new_camera["perspective_fov_deg"]
        or not np.allclose(
            old_meta["eye_from_anchor_m"], new_meta["eye_from_anchor_m"], atol=1e-6, rtol=0
        )
    ):
        raise ValueError("a rotation replay requires one fixed world-space eye and projection profile")
    centre, radius, work, root, visibility, parent = selected(
        current, new_meta["eye_from_anchor_m"]
    )
    direct = visibility[root] == 2
    start = perf_counter()
    centres, radii, ids = parent
    all_hidden = occluded(
        old_clip, old_depth, np.concatenate((centre, centres)), np.r_[radius, radii], guard_px, bias
    )
    hidden, hidden_roots = np.split(all_hidden, [len(centre)])
    hidden_parent = np.isin(root, ids[hidden_roots])
    elapsed = (perf_counter() - start) * 1000
    confirmed = occluded(new_clip, new_depth, centre[hidden], radius[hidden], 0, bias, pyramid=False)
    return {
        "triangles": int(work.sum()),
        "direct_triangles": int(work[direct].sum()),
        "hidden_triangles": int(work[hidden].sum()),
        "hidden_indirect_triangles": int(work[hidden & ~direct].sum()),
        "hidden_root_triangles": int(work[hidden_parent].sum()),
        "hidden_direct_root_triangles": int(work[hidden_parent & direct].sum()),
        "outside_current_depth_proof": int(work[hidden][~confirmed].sum()),
        "old_covered_pixels": int(np.count_nonzero(old_depth)),
        "new_covered_pixels": int(np.count_nonzero(new_depth)),
        "previous_depth_replay_ms": elapsed,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("previous", type=Path)
    parser.add_argument("current", type=Path)
    parser.add_argument("--guard-px", type=float, default=2)
    parser.add_argument("--depth-bias", type=float, default=0.0000002)
    args = parser.parse_args()
    if not all(np.isfinite(v) and v >= 0 for v in (args.guard_px, args.depth_bias)):
        parser.error("guards must be finite and nonnegative")
    print(json.dumps(replay(args.previous, args.current, args.guard_px, args.depth_bias)))


if __name__ == "__main__":
    main()
