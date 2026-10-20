"""Compare visibility group sizes on original native draw inputs and final depth."""

import argparse
import json
import time
from dataclasses import dataclass
from pathlib import Path

import numpy as np


@dataclass
class Snapshot:
    vertices: np.ndarray
    indices: np.ndarray
    jobs: np.ndarray
    models: np.ndarray
    clip: np.ndarray
    shift: np.ndarray
    planes: np.ndarray
    source: np.ndarray
    levels: list


def load(directory):
    manifest = json.loads((directory / "inputs.json").read_text())
    if not manifest["available"] or not manifest["uniform_written"]:
        raise ValueError("native input readback is incomplete")
    for name in (
        "vertices",
        "indices",
        "jobs",
        "kept",
        "arguments",
        "placements",
        "pyramid",
    ):
        entry = manifest[name]
        if (
            not entry["written"]
            or (directory / (name + ".bin")).stat().st_size != entry["bytes"]
        ):
            raise ValueError("native buffer length differs from its readback receipt")
    raw = (directory / "uniform.bin").read_bytes()
    if len(raw) != 256:
        raise ValueError("expected the native 256-byte CullView layout")
    words = np.frombuffer(raw, dtype="<u4")
    floats = words.view("<f4")
    jobs = np.fromfile(directory / "jobs.bin", dtype="<u4").reshape(-1, 4)[: words[28]]
    kept = np.fromfile(directory / "kept.bin", dtype="<u4")[: len(jobs)]
    args = np.fromfile(directory / "arguments.bin", dtype="<u4").reshape(-1, 5)
    encoded = np.zeros(len(args), dtype=bool)
    draws = json.loads((directory / "draws.json").read_text())
    if not draws["ready"]:
        raise ValueError("final draw arguments were unavailable")
    for kind, first, end, indirect in draws["encoded_ranges"]:
        if not 0 <= first <= end <= len(args):
            raise ValueError("encoded draw range exceeds argument storage")
        if kind == 0 and indirect:
            encoded[first:end] = True
    selected = (kept > 0) & encoded[jobs[:, 1]]
    chosen = jobs[selected]
    if not np.array_equal(chosen[:, 3], kept[selected]):
        raise ValueError("kept jobs do not describe complete original index ranges")
    actual = args[encoded, 0].astype(np.uint64) * args[encoded, 1]
    if int(actual.sum()) != int(chosen[:, 3].astype(np.uint64).sum()):
        raise ValueError("encoded argument counts differ from selected job counts")
    models = np.fromfile(directory / "placements.bin", dtype="<f4").reshape(-1, 36)
    models = models[args[chosen[:, 1], 4], :16].reshape(-1, 4, 4).transpose(0, 2, 1)
    packed = np.fromfile(directory / "pyramid.bin", dtype="<f4")
    levels = []
    for level in range(4):
        width, height, first = map(
            int, (words[48 + level], words[52 + level], words[56 + level])
        )
        levels.append(packed[first : first + width * height].reshape(height, width))
    vertices = np.memmap(directory / "vertices.bin", dtype="<f4", mode="r").reshape(
        -1, 3
    )
    indices = np.memmap(directory / "indices.bin", dtype="<u4", mode="r")
    return Snapshot(
        vertices,
        indices,
        chosen,
        models.astype(np.float64),
        floats[32:48].reshape(4, 4).T.astype(np.float64),
        floats[24:27].astype(np.float64),
        floats[:24].reshape(6, 4).astype(np.float64),
        words[61:63].astype(np.float64),
        extend_pyramid(levels),
    )


def extend_pyramid(levels):
    levels = list(levels)
    while levels[-1].shape != (1, 1):
        depth = levels[-1]
        height, width = depth.shape
        padded = np.pad(depth, ((0, height % 2), (0, width % 2)), mode="edge")
        levels.append(
            padded.reshape((height + 1) // 2, 2, (width + 1) // 2, 2).min(axis=(1, 3))
        )
    return levels


def groups(snapshot, triangles):
    if triangles < 0:
        raise ValueError("group limit must be nonnegative; zero retains native groups")
    lows, highs, parents, weights = [], [], [], []
    for parent, (_, _, first, count) in enumerate(snapshot.jobs):
        first, count = int(first), int(count)
        if count == 0 or count % 3 or first + count > len(snapshot.indices):
            raise ValueError("invalid original triangle range")
        indices = snapshot.indices[first : first + count]
        if int(indices.max()) >= len(snapshot.vertices):
            raise ValueError("original index exceeds position storage")
        points = snapshot.vertices[indices].astype(np.float64)
        if not np.isfinite(points).all():
            raise ValueError("nonfinite original position")
        cuts = np.arange(0, count, triangles * 3 if triangles else count)
        lows.append(np.minimum.reduceat(points, cuts, axis=0))
        highs.append(np.maximum.reduceat(points, cuts, axis=0))
        parents.extend([parent] * len(cuts))
        weights.extend(np.diff(np.append(cuts, count)) // 3)
    lower = np.concatenate(lows) if lows else np.empty((0, 3))
    upper = np.concatenate(highs) if highs else np.empty((0, 3))
    models = snapshot.models[np.asarray(parents, dtype=int)]
    return lower, upper, models, np.asarray(weights, dtype=np.uint64)


def project(snapshot, lower, upper, models):
    corners = np.array([[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)])
    points = lower[:, None, :] + corners[None, :, :] * (upper - lower)[:, None, :]
    world = np.einsum("nij,nkj->nki", models[:, :3, :3], points)
    world += models[:, :3, 3][:, None, :] + snapshot.shift
    planes = (
        np.einsum("nki,pi->nkp", world, snapshot.planes[:, :3]) + snapshot.planes[:, 3]
    )
    enabled = np.linalg.norm(snapshot.planes[:, :3], axis=1) >= 0.5
    outside = ((planes.max(axis=1) < 0) & enabled).any(axis=1)
    homogeneous = np.concatenate((world, np.ones((len(world), 8, 1))), axis=2)
    clip = np.einsum("nki,ji->nkj", homogeneous, snapshot.clip)
    safe = clip[:, :, 3].min(axis=1) > 1e-6
    ndc = clip[:, :, :3] / np.where(safe[:, None, None], clip[:, :, 3, None], 1)
    least, most = ndc.min(axis=1), ndc.max(axis=1)
    low = np.column_stack((least[:, 0] * 0.5 + 0.5, 0.5 - most[:, 1] * 0.5)).clip(0, 1)
    high = np.column_stack((most[:, 0] * 0.5 + 0.5, 0.5 - least[:, 1] * 0.5)).clip(0, 1)
    return outside, safe, low, high, most[:, 2]


def classify(snapshot, projected, weights, count):
    outside, safe, low, high, nearest = projected
    rejected = outside.copy()
    reads, capped = 0, 0
    for at in np.flatnonzero(safe & ~outside):
        across = 0.5 * max((high[at] - low[at]) * snapshot.source)
        level = 0
        while level + 1 < 4 and across > 2:
            across *= 0.5
            level += 1
        while True:
            depth = snapshot.levels[level]
            height, width = depth.shape
            blocks = snapshot.source / (2 << level)
            first = np.minimum((low[at] * blocks).astype(int), [width - 1, height - 1])
            last = np.minimum((high[at] * blocks).astype(int), [width - 1, height - 1])
            if ((last - first) < 4).all() or level + 1 >= count:
                break
            level += 1
        if ((last - first) >= 4).any():
            capped += int(weights[at])
            continue
        values = depth[first[1] : last[1] + 1, first[0] : last[0] + 1]
        reads += values.size
        rejected[at] = nearest[at] < float(values.min())
    return rejected, reads, capped


def query(snapshot, projected, weights, count):
    rejected, reads, capped = classify(snapshot, projected, weights, count)
    return {
        "levels": count,
        "rejected_triangles": int(weights[rejected].sum()),
        "remaining_triangles": int(weights[~rejected].sum()),
        "size_cap_triangles": capped,
        "pyramid_scalar_reads": int(reads),
    }


def compare(snapshot, limits):
    total = int(snapshot.jobs[:, 3].astype(np.uint64).sum()) // 3
    rows = []
    for limit in limits:
        began = time.perf_counter()
        lower, upper, models, weights = groups(snapshot, limit)
        if int(weights.sum()) != total:
            raise ValueError("grouping changed the original triangle population")
        projected = project(snapshot, lower, upper, models)
        rows.append(
            {
                "maximum_triangles": limit or None,
                "groups": len(weights),
                "selected_original_triangles": total,
                "bounds_arrays_bytes": lower.nbytes + upper.nbytes,
                "model_and_weight_arrays_bytes": models.nbytes + weights.nbytes,
                "frustum_rejected_triangles": int(weights[projected[0]].sum()),
                "queries": [
                    query(snapshot, projected, weights, count)
                    for count in (4, len(snapshot.levels))
                ],
                "python_seconds": time.perf_counter() - began,
            }
        )
    return {
        "scope": "Double cost model on actual selected native inputs and final depth; "
        "not historical-depth replay, Float32/raster proof, GPU timing or world admission",
        "comparisons": rows,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--groups", type=int, nargs="+", default=[0, 64, 32, 16, 8])
    args = parser.parse_args()
    report = compare(load(args.input), args.groups)
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
