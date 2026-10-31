"""Replay native terrain selection against a supplied current-frame depth image."""

import argparse
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np

EPSILON = np.finfo(np.float32).eps


@dataclass(frozen=True)
class Projection:
    eligible: np.ndarray
    unknown: np.ndarray
    lower: np.ndarray
    upper: np.ndarray
    nearest: np.ndarray
    minimum_w: np.ndarray
    scale: np.ndarray
    margin: np.ndarray


def project(bounds, view):
    bounds = np.asarray(bounds, dtype=np.float64)
    if bounds.ndim != 3 or bounds.shape[1:] != (3, 4):
        raise ValueError("bounds must contain three native vec4 words per patch")
    if len(view) != 192:
        raise ValueError("terrain selection view must contain 192 native bytes")
    floats = np.frombuffer(view, dtype="<f4").astype(np.float64)
    integers = np.frombuffer(view, dtype="<u4")
    matrix = floats[:16].reshape(4, 4).T
    shift, viewport = floats[16:19], floats[20:24]
    source = integers[37:39].astype(np.float64)
    if not np.isfinite(floats[:24]).all() or min(*viewport[2:], *source) <= 0:
        raise ValueError("view has invalid projection or dimensions")
    valid = (bounds[:, 0, 3] >= 0.5) & np.isfinite(bounds[:, :2]).all(axis=(1, 2))
    low = np.where(valid[:, None], bounds[:, 0, :3], 0)
    high = np.where(valid[:, None], bounds[:, 1, :3], 0)
    margin = 8 * EPSILON * (np.maximum(np.abs(low), np.abs(high)) + np.abs(shift) + 1)
    corners = np.array([[i & 1, i & 2, i & 4] for i in range(8)], dtype=bool)
    points = np.where(
        corners[None], (high + shift + margin)[:, None], (low + shift - margin)[:, None]
    )
    clip = points @ matrix[:, :3].T + matrix[:, 3]
    unsafe = (clip[:, :, 3] <= 1e-5).any(axis=1) | ~np.isfinite(clip).all(axis=(1, 2))
    with np.errstate(divide="ignore", invalid="ignore"):
        ndc = np.divide(clip[:, :, :3], clip[:, :, 3, None])
    ndc = np.where((valid & ~unsafe)[:, None, None], ndc, 0)
    least, most = ndc[:, :, :2].min(axis=1), ndc[:, :, :2].max(axis=1)
    lower = (
        viewport[:2]
        + np.column_stack((least[:, 0] * 0.5 + 0.5, 0.5 - most[:, 1] * 0.5))
        * viewport[2:]
        - 1
    )
    upper = (
        viewport[:2]
        + np.column_stack((most[:, 0] * 0.5 + 0.5, 0.5 - least[:, 1] * 0.5))
        * viewport[2:]
        + 1
    )
    outside = (upper < 0).any(axis=1) | (lower > source).any(axis=1)
    x, y, w = matrix[0, :3], matrix[1, :3], matrix[3, :3]
    loose = (
        0.5
        * viewport[2:]
        * (
            np.array([np.linalg.norm(x), np.linalg.norm(y)])
            + np.maximum(np.abs(least), np.abs(most)) * np.linalg.norm(w)
        )
    )
    scale = np.zeros(len(bounds))
    for corner in range(4):
        at = np.column_stack(
            (
                most[:, 0] if corner & 1 else least[:, 0],
                most[:, 1] if corner & 2 else least[:, 1],
            )
        )
        a = 0.5 * viewport[2] * (x - at[:, 0, None] * w)
        b = 0.5 * viewport[3] * (y - at[:, 1, None] * w)
        aa, bb, ab = np.sum(a * a, axis=1), np.sum(b * b, axis=1), np.sum(a * b, axis=1)
        scale = np.maximum(scale, np.sqrt(0.5 * (aa + bb + np.hypot(aa - bb, 2 * ab))))
    scale += 64 * EPSILON * np.linalg.norm(loose, axis=1)
    return Projection(
        eligible=valid & ~unsafe & ~outside,
        unknown=~valid | unsafe,
        lower=np.clip(lower / source, 0, 1),
        upper=np.clip(upper / source, 0, 1),
        nearest=np.clip(ndc[:, :, 2].max(axis=1), 0, 1),
        minimum_w=clip[:, :, 3].min(axis=1),
        scale=scale,
        margin=margin,
    )


def geometric_levels(bounds, view, projection):
    floats = np.frombuffer(view, dtype="<f4").astype(np.float64)
    matrix = floats[:16].reshape(4, 4).T
    selected = np.zeros(len(bounds), dtype=np.int64)
    for level in range(1, 4):
        error = bounds[:, 2, level] + 2 * np.linalg.norm(projection.margin, axis=1)
        denominator = projection.minimum_w - error * np.linalg.norm(matrix[3, :3])
        with np.errstate(invalid="ignore"):
            choose = (
                np.isfinite(error)
                & (denominator > 0)
                & (error * projection.scale <= floats[44] * denominator)
            )
        selected[choose & projection.eligible] = level
    return selected


def depth_pyramid(depth):
    depth = np.asarray(depth, dtype=np.float32)
    if (
        depth.ndim != 2
        or 0 in depth.shape
        or not np.isfinite(depth).all()
        or depth.min() < 0
        or depth.max() > 1
    ):
        raise ValueError("depth must be a nonempty finite reversed-Z image in [0,1]")
    levels = []
    for _ in range(4):
        padded = np.pad(
            depth, ((0, depth.shape[0] % 2), (0, depth.shape[1] % 2)), mode="edge"
        )
        depth = padded.reshape(padded.shape[0] // 2, 2, padded.shape[1] // 2, 2).min(
            axis=(1, 3)
        )
        levels.append(depth)
    return levels


def occluded(projection, depth):
    pyramid = depth_pyramid(depth)
    source = np.array(np.shape(depth)[::-1])
    across = 0.5 * ((projection.upper - projection.lower) * source).max(axis=1)
    levels = np.zeros(len(across), dtype=np.int64)
    for _ in range(3):
        advance = across > 2
        levels += advance
        across = np.where(advance, across * 0.5, across)
    farthest = np.ones(len(across), dtype=np.float32)
    bounded = np.zeros(len(across), dtype=bool)
    for level, field in enumerate(pyramid):
        ids = np.flatnonzero(projection.eligible & (levels == level))
        limit = np.array(field.shape[::-1]) - 1
        first = np.minimum(
            np.floor(projection.lower[ids] * source / (2 << level)).astype(int), limit
        )
        last = np.minimum(
            np.floor(projection.upper[ids] * source / (2 << level)).astype(int), limit
        )
        keep = (last - first < 4).all(axis=1)
        ids, first, last = ids[keep], first[keep], last[keep]
        bounded[ids] = True
        for y in range(4):
            for x in range(4):
                at = first + [x, y]
                inside = (at <= last).all(axis=1)
                selected, xy = ids[inside], at[inside]
                farthest[selected] = np.minimum(
                    farthest[selected], field[xy[:, 1], xy[:, 0]]
                )
    return bounded & (projection.nearest + 1e-6 < farthest)


def analyse(snapshot, depth_path, benchmark, before_depth=None):
    manifest = json.loads((snapshot / "manifest.json").read_text())
    if manifest.get("schema") != 1 or manifest.get("errors"):
        raise ValueError("snapshot capture is incomplete or has an unknown schema")
    groups = [group for group in manifest["groups"] if group.get("bounds")]
    if len(groups) != 1:
        raise ValueError("snapshot must identify one complete native terrain selection")
    group = groups[0]
    folder = snapshot / group["identity"]
    payloads = {
        name: (folder / f"{name}.bin").read_bytes() for name in ["bounds", "view"]
    }
    for name, payload in payloads.items():
        if hashlib.sha256(payload).hexdigest() != group["hashes"][name]:
            raise ValueError(f"snapshot {name} words changed")
    bounds = (
        np.frombuffer(payloads["bounds"], dtype="<f4")
        .reshape(-1, 3, 4)
        .astype(np.float64)
    )
    words = np.frombuffer(payloads["view"], dtype="<u4")
    if words[36] != len(bounds) or group["bounds"] != len(bounds):
        raise ValueError("native source count disagrees with captured bounds")
    report = json.loads(benchmark.read_text())
    if (
        not report["world_complete"]
        or not report["measurement_complete"]
        or report["unrefined_frames"]
    ):
        raise ValueError("benchmark did not finish the unchanged complete world")
    depth = np.fromfile(depth_path, dtype="<f4").reshape(int(words[38]), int(words[37]))
    projection = project(bounds, payloads["view"])
    levels = geometric_levels(bounds, payloads["view"], projection)
    metrics = {row["name"]: row["value"] for row in report["metrics"]}
    native = np.array(
        [int(metrics[f"ground: LOD{k} patch instances"]) for k in range(4)]
    )
    visible = projection.eligible | projection.unknown
    if before_depth is not None:
        previous = np.fromfile(before_depth, dtype="<f4").reshape(depth.shape)
        visible &= ~occluded(projection, previous)
    counts = np.bincount(levels[visible], minlength=4)
    if not np.array_equal(counts, native):
        raise ValueError(
            "model does not reproduce native LOD counts; check the initial depth and capture"
        )
    indices = np.array(
        [
            int(metrics[f"ground: LOD{k} index references"] / native[k])
            if native[k]
            else 0
            for k in range(4)
        ]
    )
    hidden = visible & occluded(projection, depth)
    baseline = int(indices @ counts)
    removed = int(indices[levels[hidden]].sum())
    return {
        "commit": report["build"]["commit"],
        "native_lod_counts": native.tolist(),
        "model_lod_counts_before_depth": counts.tolist(),
        "native_counts_match": bool(np.array_equal(counts, native)),
        "input_patches": len(bounds),
        "unknown_patches": int(projection.unknown.sum()),
        "occluded_patches": int(hidden.sum()),
        "baseline_patch_indices": baseline,
        "removed_patch_indices": removed,
        "removed_index_percent": 100 * removed / baseline if baseline else 0,
        "depth_sha256": hashlib.sha256(depth_path.read_bytes()).hexdigest(),
        "initial_depth_supplied": before_depth is not None,
        "scope": "Current final depth is a hypothetical occluder; no prepass cost, new bias or native default is qualified.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--depth", type=Path, required=True)
    parser.add_argument("--benchmark", type=Path, required=True)
    parser.add_argument("--before-depth", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyse(args.snapshot, args.depth, args.benchmark, args.before_depth)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
