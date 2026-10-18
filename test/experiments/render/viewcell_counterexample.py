"""Rendered movement counterexample; analytic fixture, not native-world acceptance."""

import argparse
import itertools
import json
import tempfile
from pathlib import Path

import numpy as np
from current_visibility import project, raster
from outshine_ref.geometry.occlusion import convex_patch_umbra
from outshine_ref.geometry.occlusion_hierarchy import BoundsHierarchy
from PIL import Image

WALL = np.array([[-1, 0, -1], [1, 0, -1], [1, 0, 1], [-1, 0, 1]], dtype=float)
BOX = np.array([[1.95, 2.95, -0.05], [2.05, 3.05, 0.05]])


def emit_box(bounds):
    vertices = np.array(
        list(itertools.product(*zip(bounds[0], bounds[1], strict=True)))
    )
    triangles = []
    for axis in range(3):
        others = [(axis + 1) % 3, (axis + 2) % 3]
        for side in (0, 1):
            ids = []
            for a, b in ((0, 0), (1, 0), (1, 1), (0, 1)):
                point = bounds[0].copy()
                point[axis] = bounds[side, axis]
                point[others] = [bounds[a, others[0]], bounds[b, others[1]]]
                ids.append(int(np.flatnonzero((vertices == point).all(axis=1))[0]))
            if side == 0:
                ids.reverse()
            triangles.extend(([ids[0], ids[1], ids[2]], [ids[0], ids[2], ids[3]]))
    return vertices, np.array(triangles)


def render(eye, keep_box):
    view = (
        np.asarray(eye),
        np.array([[1, 0, 0], [0, 0, 1], [0, 1, 0]]),
        640,
        1280,
        720,
    )
    models = [(WALL, np.array([[0, 1, 2], [0, 2, 3]]), [120, 120, 120])]
    if keep_box:
        vertices, triangles = emit_box(BOX)
        models.append((vertices, triangles, [230, 65, 40]))
    depth, colour = np.zeros((720, 1280)), np.full((720, 1280, 3), 25, dtype=np.uint8)
    for vertices, triangles, rgb in models:
        projected, z = project(vertices, view)
        candidate = raster(projected, z, triangles, 1280, 720)
        take = candidate > depth
        depth[take], colour[take] = candidate[take], rgb
    return colour


def run(output):
    hierarchy = BoundsHierarchy(
        np.array([[WALL.min(axis=0), WALL.max(axis=0)], BOX]),
        known=np.ones(2, dtype=bool),
    )
    fixed = convex_patch_umbra(WALL, [[0, -2, 0], [0, -2, 0]], certified_opaque=True)
    moving = convex_patch_umbra(WALL, [[-1, -3, -1], [1, -1, 1]], certified_opaque=True)
    static_plan, cell_plan = (
        hierarchy.select([(fixed, 0)]),
        hierarchy.select([(moving, 0)]),
    )
    if (
        not static_plan.hidden[1]
        or cell_plan.hidden[1]
        or static_plan.contains_observer([1, -1, 0])
    ):
        raise ValueError(
            "fixture must expose invalid single-view reuse while retaining cell coverage"
        )
    cases = []
    for name, eye, keep in (
        ("fixed-full", [0, -2, 0], True),
        ("fixed-selected", [0, -2, 0], not static_plan.hidden[1]),
        ("moved-full", [1, -1, 0], True),
        ("moved-invalid-point-reuse", [1, -1, 0], not static_plan.hidden[1]),
        ("moved-cell-selected", [1, -1, 0], not cell_plan.hidden[1]),
    ):
        image = render(eye, keep)
        Image.fromarray(image).save(output / f"{name}.png")
        cases.append({"name": name, "emitted_triangles": 14 if keep else 2})
    full = np.asarray(Image.open(output / "moved-full.png"))
    wrong = np.asarray(Image.open(output / "moved-invalid-point-reuse.png"))
    correct = np.asarray(Image.open(output / "moved-cell-selected.png"))
    changed = int((full != wrong).any(axis=2).sum())
    if changed == 0 or not np.array_equal(full, correct):
        raise ValueError(
            "movement counterexample or conservative selected image failed"
        )
    if not np.array_equal(
        np.asarray(Image.open(output / "fixed-full.png")),
        np.asarray(Image.open(output / "fixed-selected.png")),
    ):
        raise ValueError("fixed-view selected image differs")
    (output / "report.json").write_text(
        json.dumps(
            {
                "cases": cases,
                "invalid_reuse_changed_pixels": changed,
                "cell_changed_pixels": 0,
                "profile": [1280, 720],
                "scope": "Declared opaque-wall/cuboid fixture; geometry generated only after selection. Software opaque pixel-centre raster, not native materials/world/cost or the full #12 real-scene comparison.",
            },
            indent=2,
        )
        + "\n"
    )
    print(
        json.dumps({"invalid_reuse_changed_pixels": changed, "cell_changed_pixels": 0}),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    checkout = Path(__file__).resolve().parents[3]
    if (
        target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(checkout)
        or target.exists()
    ):
        parser.error("fresh explicit system-temp output outside the checkout required")
    target.mkdir(parents=True)
    run(target)
