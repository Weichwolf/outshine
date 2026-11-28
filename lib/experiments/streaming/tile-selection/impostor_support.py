"""Replay grouped capture support admission using stored building envelope metadata."""

import argparse
import json
import math
import sqlite3
import xml.etree.ElementTree as ET
from contextlib import closing
from pathlib import Path
from time import perf_counter

import numpy as np
from tile_selection import camera_path, local_frame


def cached_cells(database, anchor, radius):
    bounds = "a.minx,a.miny,a.minz,a.maxx,a.maxy,a.maxz"
    search = tuple(float(x + d) for x in anchor for d in (radius, -radius))
    with closing(
        sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        rows = db.execute(
            f"""
            SELECT {bounds}, MAX(a.bytes)
            FROM bounds b JOIN assets a ON a.id=b.id
            WHERE b.minx<=? AND b.maxx>=? AND b.miny<=? AND b.maxy>=?
              AND b.minz<=? AND b.maxz>=? AND a.kind='building-forms'
            GROUP BY {bounds}
            """,
            search,
        ).fetchall()
    if not rows:
        raise ValueError("no cached envelopes")
    data = np.asarray(rows)
    boxes = data[:, :6].reshape(-1, 2, 3)
    assert np.all(np.isfinite(boxes)) and np.all(boxes[:, 0] <= boxes[:, 1])
    return boxes, data[:, 6].astype(np.int64)


def lens(report):
    view = report["motion"]["start"]
    width, height = (report["profile"][axis] for axis in ("width_px", "height_px"))
    focal = height / (2 * math.tan(math.radians(view["perspective_fov_deg"]) / 2))
    return focal, math.hypot(1, (width + 1) / (2 * focal), (height + 1) / (2 * focal))


def support_bound(extent, axial_distance, eye_radius, focal, scale):
    # A visible endpoint is at depth >= its radial distance / scale.
    # The other endpoint can be closer by at most the axial projection offset.
    span = extent[..., 2]
    visible_depth = (axial_distance - eye_radius - span) / scale
    other_depth = visible_depth - span
    lateral = np.linalg.norm(extent[..., :2], axis=-1) + eye_radius
    out = np.full_like(axial_distance, np.inf)
    return np.divide(
        math.sqrt(2) * focal * span * lateral,
        visible_depth * other_depth,
        where=other_depth > 0,
        out=out,
    )


def capture_extents(boxes, eye, up, focal):
    centre = boxes.mean(axis=1)
    half = (boxes[:, 1] - boxes[:, 0]) * 0.5
    delta = eye - centre
    distance = np.linalg.norm(delta, axis=1)
    toward = np.divide(
        delta, distance[:, None], out=np.zeros_like(delta), where=distance[:, None] > 0
    )
    right = np.cross(up, toward)
    magnitude = np.linalg.norm(right, axis=1)
    if np.any(magnitude == 0):
        right[magnitude == 0] = np.cross(up, np.eye(3)[np.argmin(np.abs(up))])
        magnitude = np.linalg.norm(right, axis=1)
    right /= magnitude[:, None]
    axes = np.stack((right, np.cross(toward, right), toward), axis=1)
    tight = np.einsum("nij,nj->ni", np.abs(axes), half)
    radius = np.max(tight * [1, 1, 0.5], axis=1)
    # Two capture texels per presentation pixel and a one-texel border.
    samples = (
        np.ceil(
            4
            * focal
            * radius
            / np.maximum(distance - np.linalg.norm(half, axis=1), 1e-9)
        )
        + 2
    )
    samples = np.exp2(np.ceil(np.log2(np.clip(samples, 4, 8192))))
    half_extent = radius * samples / (samples - 2)
    full = half_extent[:, None] * np.asarray([1, 1, 2])
    return tight, full, distance, samples


def verify_projection():
    rng = np.random.default_rng(1958)
    count = 16384
    extent = rng.uniform(0.05, 200, (count, 3))
    distance = rng.uniform(1, 3000, count)
    guard = rng.uniform(0, 72, count)
    eye = rng.normal(size=(count, 3))
    eye *= (rng.uniform(0, 1, count) * guard / np.linalg.norm(eye, axis=1))[:, None]
    eye[:, 2] += distance
    point = rng.uniform(-1, 1, extent.shape) * extent
    support = point.copy()
    support[:, 2] = 0
    ray, flat = point - eye, support - eye
    focal, width, height = 691.5535657096197, 1280, 720
    scale = math.hypot(1, (width + 1) / (2 * focal), (height + 1) / (2 * focal))
    forward = np.where(rng.uniform(0, 1, (count, 1)) < 0.5, ray, flat)
    forward /= np.linalg.norm(forward, axis=1)[:, None]
    right = np.cross([0, 1, 0], forward)
    right /= np.linalg.norm(right, axis=1)[:, None]
    forward += (
        right * rng.uniform(-1.1, 1.1, (count, 1)) * width
        + np.cross(forward, right) * rng.uniform(-1.1, 1.1, (count, 1)) * height
    ) / (2 * focal)
    forward /= np.linalg.norm(forward, axis=1)[:, None]
    right = np.cross([0, 1, 0], forward)
    right /= np.linalg.norm(right, axis=1)[:, None]
    basis = np.stack((right, np.cross(forward, right), forward), axis=1)
    a, b = (np.einsum("nij,nj->ni", basis, r) for r in (ray, flat))
    with np.errstate(divide="ignore", invalid="ignore"):
        image_a, image_b = (focal * v[:, :2] / v[:, 2:] for v in (a, b))
    visible = (np.abs(image_a) <= [640.5, 360.5]).all(axis=1) | (
        np.abs(image_b) <= [640.5, 360.5]
    ).all(axis=1)
    bound = support_bound(extent, distance, guard, focal, scale)
    eligible = visible & np.isfinite(bound)
    assert eligible.sum() > count // 2
    assert np.all(a[eligible, 2] > 0) and np.all(b[eligible, 2] > 0)
    displacement = np.abs(image_a[eligible] - image_b[eligible]).sum(axis=1)
    assert np.all(displacement <= bound[eligible] * (1 + 1e-10))
    return int(eligible.sum())


def measure(boxes, size, eye, eye_radius, up, focal, scale):
    began = perf_counter()
    tight, full, distance, samples = capture_extents(boxes, eye, up, focal)
    cube = support_bound(full, distance, eye_radius, focal, scale)
    source = support_bound(tight, distance, eye_radius, focal, scale)
    assert (source <= cube).all()
    near = (distance - eye_radius - full[:, 2]) / scale - full[:, 2]
    legacy_radius = np.linalg.norm(boxes[:, 1] - boxes[:, 0], axis=1)
    legacy_distance = distance - legacy_radius - eye_radius

    def selected(error, limit, before_metadata=False):
        mask = (error <= limit) & (samples <= 4096) & (near > 0)
        if before_metadata:
            mask &= (legacy_distance > 0) & (
                focal * legacy_radius**2 <= limit * legacy_distance**2
            )
        return [int(mask.sum()), int(size[mask].sum()), int(np.sum(samples[mask] ** 2))]

    return {
        "eye_radius_m": eye_radius,
        "thresholds_px": {
            px: {
                "full_capture": selected(cube, px),
                "source_envelope": selected(source, px),
                "source_envelope_after_legacy_prefilter": selected(source, px, True),
            }
            for px in (1, 2, 4)
        },
        "selection_ms": (perf_counter() - began) * 1000,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument("scenario", type=Path)
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--radius-m", type=float, default=12000)
    args = parser.parse_args()
    if not math.isfinite(args.radius_m) or args.radius_m <= 0:
        parser.error("expected a positive finite search radius")
    world = ET.parse(args.scenario).getroot().find("world")
    anchor, axes = local_frame(float(world.attrib["lat"]), float(world.attrib["lon"]))
    boxes, size = cached_cells(args.database, anchor, args.radius_m)
    route = json.loads(args.benchmark.read_text())
    start, end = camera_path(route, anchor, axes)
    focal, scale = lens(route)
    # Rebase before computing extents to preserve small local intervals.
    boxes, start, end = boxes - anchor, start - anchor, end - anchor
    up = axes[:, 1]
    print(
        json.dumps(
            {
                "checked_visible_pairs": verify_projection(),
                "cached_envelopes": len(size),
                "columns": ["cells", "form_bytes", "capture_texels"],
                "at_start": measure(boxes, size, start, 0, up, focal, scale),
                "local_reuse": measure(boxes, size, start, 32, up, focal, scale),
                "motion": measure(
                    boxes,
                    size,
                    (start + end) / 2,
                    float(np.linalg.norm(end - start)) / 2,
                    up,
                    focal,
                    scale,
                ),
            }
        )
    )


if __name__ == "__main__":
    main()
