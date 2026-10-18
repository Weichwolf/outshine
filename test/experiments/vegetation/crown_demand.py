"""Analytic original-host envelopes and explicit pose demand before organ meshes."""

import numpy as np
from outshine_ref.vegetation.curve_frame import arc_frame
from outshine_ref.vegetation.shoot_plan import normal_frame


def host_bounds(source, *, method="arc"):
    if method not in ("root", "arc"):
        raise ValueError("declared root or arc envelope required")
    crown = source.crown
    origins = crown.positions[crown.hosts]
    # Includes whole connected carriers at the largest admitted padding(size16).
    reach = crown.petiole_m + max(
        b.length + b.petiole + b.width / 2 * (4 / 3) * np.hypot(1, b.fold)
        for b in source.blades
    )
    if method == "root":
        lower, upper = (
            origins - (crown.length_m + reach),
            origins + (crown.length_m + reach),
        )
    else:
        frames = normal_frame(
            origins - crown.positions[crown.parents[crown.hosts]], (0, 0, 1)
        )
        # A coordinate derivative is frameY*cos(bend*t)+frameZ*sin(bend*t).
        phase = np.arctan2(-frames[:, :, 1], frames[:, :, 2])
        angles = phase[..., None] + np.arange(-1, 2) * np.pi
        t = np.clip(angles / (crown.bend_rad or 1), 0, 1).reshape(len(origins), -1)
        t = np.column_stack((np.zeros(len(origins)), np.ones(len(origins)), t))
        centre = arc_frame(crown.length_m, crown.bend_rad, 0, t)[0]
        points = origins[:, None] + np.einsum("nij,nkj->nki", frames, centre)
        lower, upper = points.min(axis=1) - reach, points.max(axis=1) + reach
    guard = (
        256
        * np.finfo(float).eps
        * (np.max(abs(origins), axis=1) + crown.length_m + reach + 1)
    )
    lower = np.nextafter(lower - guard[:, None], -np.inf)
    upper = np.nextafter(upper + guard[:, None], np.inf)
    lower.setflags(write=False)
    upper.setflags(write=False)
    return lower, upper


def projected_spans(camera, lower, upper):
    centre, extent = (lower + upper) / 2, (upper - lower) / 2
    local = (centre - camera.position) @ camera.axes.T
    depth = local[:, 2] - extent @ abs(camera.axes[2])
    denominator = np.where(depth > camera.near, depth, 1.0)
    horizontal = np.linalg.norm(local[:, :2], axis=1) + np.linalg.norm(extent, axis=1)
    focal = camera.height / (2 * np.tan(np.deg2rad(camera.fov_y) / 2))
    span = (
        focal
        * np.linalg.norm(upper - lower, axis=1)
        / denominator
        * np.hypot(1, horizontal / denominator)
    )
    return np.where(depth > camera.near, span, np.inf)


class CrownDemand:
    def __init__(self, source, *, method="arc"):
        self.source, self.method = source, method
        self.lower, self.upper = host_bounds(source, method=method)

    def query(self, cameras, *, max_leaves, min_span_px=0.0):
        if type(max_leaves) is not int or not 0 <= max_leaves <= 100000:
            raise ValueError("explicit bounded leaf-pose capacity required")
        if (
            not np.isfinite(min_span_px)
            or min_span_px < 0
            or not 1 <= len(cameras) <= 16
        ):
            raise ValueError(
                "one to16 views and finite nonnegative detail threshold required"
            )
        visible = np.zeros(len(self.lower), bool)
        span = np.zeros(len(self.lower))
        for camera in cameras:
            mask = camera.visible(self.lower, self.upper)
            visible |= mask
            span = np.maximum(
                span, np.where(mask, projected_spans(camera, self.lower, self.upper), 0)
            )
        counts = self.source.crown.counts
        detail = visible & (span >= min_span_px) & (counts > 0)
        count = int(counts[detail].sum())
        return {
            "visible": visible,
            "detail": detail,
            "projected_span_px": span,
            "requested_leaf_poses": count,
            "accepted": count <= max_leaves,
        }

    def prepare(self, cameras, *, max_leaves, min_span_px=0.0):
        query = self.query(cameras, max_leaves=max_leaves, min_span_px=min_span_px)
        if not query["accepted"]:
            return query, None
        crown = self.source.crown
        count = query["requested_leaf_poses"]
        poses = {
            "ids": np.empty(count, np.uint64),
            "origins": np.empty((count, 3)),
            "frames": np.empty((count, 3, 3)),
            "variants": np.empty(count, np.uint8),
        }
        cursor = 0
        for node in crown.hosts[query["detail"]]:
            crown._groups.clear()
            group = crown.group(int(node), max_leaves)
            stop = cursor + len(group["ids"])
            for key, values in poses.items():
                values[cursor:stop] = group[key]
            cursor = stop
        assert cursor == count
        for v in poses.values():
            v.setflags(write=False)
        return query, poses
