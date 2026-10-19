"""Independent triangle-image control for an eye inside a loose global AABB."""

import argparse
import hashlib
import json
import tempfile
from pathlib import Path

import numpy as np
from building_trace_bounds import (
    local_part_bounds,
    ray_candidates,
    source_local_rays,
    union_bound,
)
from PIL import Image
from viewcell_counterexample import emit_box

WIDTH, HEIGHT = 1280, 720


def source_model():
    c = np.sqrt(0.5)
    surface = {
        "projectable": True,
        "origin": [2.0, 0, 0],
        "east": [c, 0, c],
        "north": [0, 1.0, 0],
        "up": [-c, 0, c],
        "shapes": [
            {
                "ring": [[-0.2, -0.4], [0.2, -0.4], [0.2, 0.4], [-0.2, 0.4]],
                "holes": [],
                "roof_kind": 0,
                "rise_m": 0.0,
                "bottom_m": -4.0,
                "roof_deck_m": 4.0,
            }
        ],
    }
    vertices, indices = emit_box(np.array([[-0.2, -0.4, -4.0], [0.2, 0.4, 4.0]]))
    axes = np.array([surface[k] for k in ("east", "north", "up")])
    vertices = surface["origin"] + vertices @ axes
    return surface, vertices, indices


def triangle_hits(vertices, indices, eye, directions):
    """Independent front-facing Moller/Trumbore intersections, no box tests."""
    closest = np.full(len(directions), np.inf)
    owners = np.full(len(directions), -1, dtype=int)
    for index, triangle in enumerate(vertices[indices]):
        a, b = triangle[1] - triangle[0], triangle[2] - triangle[0]
        p = np.cross(directions, b)
        determinant = p @ a
        usable = determinant > 0
        inverse = np.divide(
            1.0, determinant, out=np.zeros(len(directions)), where=usable
        )
        offset = eye - triangle[0]
        u = (p @ offset) * inverse
        q = np.cross(offset, a)
        v = (directions @ q) * inverse
        along = np.dot(b, q) * inverse
        hit = (
            usable
            & (u >= 0)
            & (v >= 0)
            & (u + v <= 1)
            & (along > 0)
            & (along < closest)
        )
        closest[hit], owners[hit] = along[hit], index
    return closest, owners


def run(output):
    surface, vertices, indices = source_model()
    bound = union_bound(local_part_bounds(surface, padding_m=0.25))
    global_box = np.stack((vertices.min(axis=0), vertices.max(axis=0)))
    right, up, forward = (
        -np.asarray(surface["north"]),
        np.asarray(surface["up"]),
        np.asarray(surface["east"]),
    )
    colours = np.array(
        [
            [70 + 10 * (i // 2), 125 + 8 * (i // 2), 195 + 5 * (i // 2)]
            for i in range(len(indices))
        ],
        dtype=np.uint8,
    )
    results = []
    for pose, eye in enumerate(
        (np.zeros(3), np.array([0.0, 0.125, 0.0]), np.array([0.1, -0.1, 0.05]))
    ):
        if not ((eye >= global_box[0]) & (eye <= global_box[1])).all():
            raise ValueError("fixture eye must be inside the original global AABB")
        full_depth, new_depth = (
            np.full(WIDTH * HEIGHT, np.inf),
            np.full(WIDTH * HEIGHT, np.inf),
        )
        full_owner, new_owner = np.full(WIDTH * HEIGHT, -1), np.full(WIDTH * HEIGHT, -1)
        tested = 0
        for first in range(0, WIDTH * HEIGHT, 65536):
            ids = np.arange(first, min(first + 65536, WIDTH * HEIGHT))
            y, x = ids // WIDTH, ids % WIDTH
            directions = (
                right[None] * ((x + 0.5 - WIDTH / 2) / 360)[:, None]
                + up[None] * ((HEIGHT / 2 - y - 0.5) / 360)[:, None]
                + forward
            )
            # The actual native placement remains after RequireShapes. These
            # immutable fixture parameters are finite/validated before filtering.
            if not np.isfinite(vertices).all():
                raise ValueError("source 1: required geometry validation failed")
            local_origin, local_direction = source_local_rays(surface, eye, directions)
            keep = ray_candidates(bound, local_origin, local_direction)
            depth, owner = triangle_hits(vertices, indices, eye, directions)
            selected_depth, selected_owner = triangle_hits(
                vertices, indices, eye, directions[keep]
            )
            full_depth[ids], full_owner[ids] = depth, owner
            new_depth[ids[keep]], new_owner[ids[keep]] = selected_depth, selected_owner
            tested += int(keep.sum())
        if not np.array_equal(full_owner, new_owner) or not np.array_equal(
            full_depth, new_depth
        ):
            raise ValueError(
                "local source guard changes a triangle owner or image depth"
            )
        visible = full_owner >= 0
        if visible.sum() < 100 or not 0 < tested < WIDTH * HEIGHT:
            raise ValueError("fixture must show preserved geometry and avoided work")
        for name, owners in (("original", full_owner), ("guarded", new_owner)):
            rgb = np.full((WIDTH * HEIGHT, 3), 20, dtype=np.uint8)
            ids = owners >= 0
            rgb[ids] = colours[owners[ids]]
            Image.fromarray(rgb.reshape(HEIGHT, WIDTH, 3)).save(
                output / f"pose-{pose}-{name}.png"
            )
        results.append(
            {
                "eye": eye.tolist(),
                "original_rays": WIDTH * HEIGHT,
                "guarded_expensive_queries": tested,
                "visible_pixels": int(visible.sum()),
                "triangles_unchanged": len(indices),
                "changed_owner_pixels": 0,
                "changed_depth_pixels": 0,
                "changed_colour_pixels": 0,
            }
        )
    report = {
        "scope": "Analytic rotated thin cuboid, original AABB contains the camera while the outer local bound does not. Independent all-triangle queries before/after guard produce identical nearest triangle/depth/RGB. Six actual PNGs, three independently recomputed eyes, full original geometry retained. Immutable geometry validation precedes every filtered batch; single-ray source/error ordering is separately tested. No original native full capture/framebuffer/device or global-ray equivalence proof.",
        "dimensions": [WIDTH, HEIGHT],
        "source_global_bounds": global_box.tolist(),
        "source_local_bound": bound.tolist(),
        "poses": results,
        "code_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                Path(__file__).name,
                "building_trace_bounds.py",
                "viewcell_counterexample.py",
            )
        },
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        target.exists()
        or target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    target.mkdir(parents=True)
    print(json.dumps(run(target)), flush=True)
