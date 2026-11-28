"""Compare bounded local river-height approximations; this is not a native water recipe."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely
from shapely.strtree import STRtree


class RiverProfile:
    def __init__(self, segments, levels, segment_limit=8192):
        self.segments = np.asarray(segments, dtype=float)
        self.levels = np.asarray(levels, dtype=float)
        count = len(self.segments)
        if not 0 < count <= segment_limit or self.segments.shape != (count, 2, 2):
            raise ValueError("bounded nonempty pairs of planar segment endpoints required")
        if self.levels.shape != (count, 2):
            raise ValueError("one ASL height per segment endpoint required")
        if not np.isfinite(self.segments).all() or not np.isfinite(self.levels).all():
            raise ValueError("finite coordinates and heights required")
        self.directions = self.segments[:, 1] - self.segments[:, 0]
        self.lengths_squared = np.sum(self.directions**2, axis=1)
        if not np.isfinite(self.lengths_squared).all() or np.any(self.lengths_squared == 0):
            raise ValueError("representable nonzero segment lengths required")
        self.lines = shapely.linestrings(self.segments)
        self.tree = STRtree(self.lines)

    def sample(self, points, method, blend_m=12.0, reference_limit=64):
        points = np.asarray(points, dtype=float)
        if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
            raise ValueError("finite planar query points required")
        if method not in ("nearest", "blend-clamped", "blend-affine"):
            raise ValueError("unknown height approximation")
        if not np.isfinite(blend_m) or blend_m <= 0 or reference_limit <= 0:
            raise ValueError("positive blend distance and reference limit required")
        height = np.full(len(points), np.nan)
        counts = np.zeros(len(points), dtype=int)
        for first in range(0, len(points), 64):
            batch = points[first:first + 64]
            query = shapely.points(batch)
            nearest, distances = self.tree.query_nearest(query, return_distance=True)
            minimum = np.full(len(batch), np.inf)
            np.minimum.at(minimum, nearest[0], distances)
            radius = blend_m + 2 * minimum
            pairs = (nearest if method == "nearest" else
                     self.tree.query(query, predicate="dwithin", distance=radius))
            row, segment = pairs
            references = np.bincount(row, minlength=len(batch))
            counts[first:first + len(batch)] = references
            allowed = references[row] <= reference_limit
            row, segment = row[allowed], segment[allowed]
            if not len(row):
                continue
            direction = self.directions[segment]
            t = np.sum((batch[row] - self.segments[segment, 0]) * direction, axis=1)
            t /= self.lengths_squared[segment]
            if method != "blend-affine":
                t = np.clip(t, 0, 1)
            values = self.levels[segment, 0] * (1 - t) + self.levels[segment, 1] * t
            weights = np.ones(len(row))
            if method != "nearest":
                ratio = np.clip(shapely.distance(query[row], self.lines[segment]) / radius[row], 0, 1)
                weights = (1 - ratio)**3 * (1 + 3 * ratio + 6 * ratio**2)
            total = np.bincount(row, weights=weights, minlength=len(batch))
            weighted = np.bincount(row, weights=weights * values, minlength=len(batch))
            valid = total > 0
            height[first + np.flatnonzero(valid)] = weighted[valid] / total[valid]
        return height, counts


def flow_check(profile, segments, levels, method, blend_m=12.0, reference_limit=64):
    segments = np.asarray(segments).copy()
    levels = np.asarray(levels).copy()
    reverse = levels[:, 0] < levels[:, 1]
    segments[reverse] = segments[reverse, ::-1]
    levels[reverse] = levels[reverse, ::-1]
    segments = segments[levels[:, 0] - levels[:, 1] > .01]
    fractions = np.linspace(0, 1, 17)
    points = (segments[:, 0, None, :] +
              (segments[:, 1] - segments[:, 0])[:, None, :] * fractions[None, :, None])
    values, _ = profile.sample(points.reshape(-1, 2), method, blend_m, reference_limit)
    values = values.reshape(len(segments), 17)
    valid = np.isfinite(values).all(axis=1)
    values = values[valid]
    rebound = values - np.minimum.accumulate(values, axis=1)
    return {
        "source_drop_minimum_m": .01,
        "samples_per_segment": 17,
        "checked_segments": int(valid.sum()),
        "unused_segments": int((~valid).sum()),
        "segments_with_sampled_uphill": int((np.diff(values, axis=1).max(axis=1) > 1e-4).sum()),
        "maximum_sampled_rebound_m": float(rebound.max(initial=0)),
        "scope": "sampled counterexample check, not a global monotonicity proof",
    }


def compare(data, blend_m, reference_limit):
    began = time.perf_counter()
    profile = RiverProfile(data["segment_nodes"], data["segment_levels"])
    prepared_ms = (time.perf_counter() - began) * 1000
    results = {}
    values = {}
    for method in ("nearest", "blend-clamped", "blend-affine"):
        began = time.perf_counter()
        height, references = profile.sample(data["boundary"], method, blend_m, reference_limit)
        elapsed_ms = (time.perf_counter() - began) * 1000
        valid = np.isfinite(height)
        values[method] = height
        results[method] = {
            "query_ms": elapsed_ms,
            "used_points": int(valid.sum()),
            "unused_points": int((~valid).sum()),
            "enumerated_references": int(references.sum()),
            "maximum_references": int(references.max(initial=0)),
            "delta_from_flat_m_range": (
                [float(np.min(height[valid] - data["old"][valid])),
                 float(np.max(height[valid] - data["old"][valid]))] if valid.any() else None),
        }
        results[method]["flow"] = flow_check(
            profile, data["segment_nodes"], data["segment_levels"], method,
            blend_m, reference_limit)
    return {"preparation_ms": prepared_ms, "methods": results}, values


def plot(data, values, output):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    xy = data["boundary"]
    heights = np.concatenate([data["old"], *values.values()])
    finite = heights[np.isfinite(heights)]
    low, high = float(np.min(finite)), float(np.max(finite))
    margin = max(1.0, (high - low) * .05)
    fig, axes = plt.subplots(1, 3, figsize=(18, 6), layout="constrained",
                             subplot_kw={"projection": "3d"})
    for ax, (name, height) in zip(axes, [("flat source fragments", data["old"]),
                                      ("nearest", values["nearest"]),
                                      ("blend-affine", values["blend-affine"])]):
        ax.scatter(xy[:, 0], xy[:, 1], height, c=height, s=1, cmap="viridis", vmin=low, vmax=high)
        ax.set(xlim=(xy[:, 0].min(), xy[:, 0].max()),
               ylim=(xy[:, 1].min(), xy[:, 1].max()), zlim=(low - margin, high + margin),
               xlabel="East [m]", ylabel="North [m]", zlabel="ASL [m]", title=name)
        ax.view_init(elev=20, azim=-50)
    fig.savefig(output / "profiles.png", dpi=130)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--blend-m", type=float, default=12.0)
    parser.add_argument("--references", type=int, default=64)
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    checkout = Path(__file__).resolve().parents[3]
    if output.is_relative_to(checkout):
        parser.error("private results belong outside the checkout")
    with np.load(args.input, allow_pickle=False) as data:
        report, values = compare(data, args.blend_m, args.references)
        report.update(input_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
                      source_segments=len(data["segment_nodes"]),
                      query_points=len(data["boundary"]),
                      blend_m=args.blend_m, reference_limit=args.references,
                      scope="experimental field only; no native mesh, terrain or frame admission")
        output.mkdir(parents=True, exist_ok=True)
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        np.savez_compressed(output / "heights.npz", **values)
        if args.plot:
            plot(data, values, output)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
