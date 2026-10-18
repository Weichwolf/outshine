"""Rendered diagonal-gap counterexample; analytic roof fixture, not a native scene."""

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
from roof_fusion import fused_roof_umbra
from viewcell_counterexample import emit_box

ROOF = np.array([[-2, -2, 0], [2, -2, 0], [2, 2, 2**-30], [-2, 2, 0]], float)
ROOF_TRIANGLES = np.array([[0, 1, 2], [0, 2, 3]])
TARGET = np.array([[-0.2, -0.2, -2], [0.2, 0.2, -1]])
OBSERVER = np.array([[-0.1, -0.1, 4], [0.1, 0.1, 5]])


def image(eye, keep_target, *, keep_roof=True):
    view = (np.asarray(eye), np.diag([1, 1, -1]), 640, 1280, 720)
    models = [(ROOF, ROOF_TRIANGLES, [125, 150, 175])] if keep_roof else []
    if keep_target:
        vertices, indices = emit_box(TARGET)
        models.append((vertices, indices, [230, 65, 40]))
    depth = np.zeros((720, 1280))
    colour = np.full((720, 1280, 3), 25, dtype=np.uint8)
    for vertices, indices, rgb in models:
        projected, z = project(vertices, view)
        candidate = raster(projected, z, indices, 1280, 720)
        take = candidate > depth
        depth[take], colour[take] = candidate[take], rgb
    return depth, colour


def run(output):
    triangles = ROOF[ROOF_TRIANGLES]
    tree = BoundsHierarchy(
        np.array([[ROOF.min(axis=0), ROOF.max(axis=0)], TARGET]),
        known=np.ones(2, dtype=bool),
    )
    singles = [
        (convex_patch_umbra(triangle, OBSERVER, certified_opaque=True), 0)
        for triangle in triangles
    ]
    fused = fused_roof_umbra(triangles, OBSERVER, certified_opaque=True)
    before = tree.select(singles)
    after = tree.select([(fused, 0)])
    if before.hidden.any() or not after.hidden[1] or after.hidden[0]:
        raise ValueError(
            "fixture must distinguish single-triangle gaps from certified joined roof"
        )
    eyes = list(itertools.product(*zip(*OBSERVER, strict=True))) + [
        OBSERVER.mean(axis=0)
    ]
    rows = []
    for index, eye in enumerate(eyes):
        if not after.contains_observer(eye):
            raise ValueError("fixture eye escapes the certificate region")
        control_depth, control = image(eye, True)
        selected_depth, selected = image(eye, not after.hidden[1])
        target_depth, target_only = image(eye, True, keep_roof=False)
        target_pixels = int((target_depth > 0).sum())
        if not target_pixels:
            raise ValueError("unoccluded fixture target must actually rasterize")
        if not np.array_equal(control_depth, selected_depth) or not np.array_equal(
            control, selected
        ):
            raise ValueError("fusion changes roof fixture depth or colour")
        for name, value in (
            ("whole", control),
            ("fused", selected),
            ("target-only", target_only),
        ):
            Image.fromarray(value).save(output / f"pose-{index}-{name}.png")
        rows.append(
            {
                "eye": np.asarray(eye).tolist(),
                "covered_pixels": int((control_depth > 0).sum()),
                "unoccluded_target_pixels": target_pixels,
                "changed_depth_pixels": 0,
                "changed_colour_pixels": 0,
            }
        )
    report = {
        "scope": "Analytic accepted opaque roof/cuboid fixture, Float64 software pixel-centre raster. No real-source, native/device or full-world acceptance.",
        "roof_triangles": 2,
        "whole_triangles": 14,
        "single_triangles": 14,
        "fused_triangles": 2,
        "observer_box": OBSERVER.tolist(),
        "poses": rows,
        "single_work": before.work,
        "fused_work": after.work,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "whole_triangles": 14,
                "single_triangles": 14,
                "fused_triangles": 2,
                "poses": len(rows),
                "changed_pixels": 0,
            }
        ),
        flush=True,
    )


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
    run(target)
