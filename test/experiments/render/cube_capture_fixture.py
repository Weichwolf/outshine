"""Visible-output control for skipping irrelevant Fine-only capture queries."""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from compare_cube_capture_masks import native_rectangle
from cube_capture_mask import (
    capture_rectangle,
    occupied_tiles,
    selected_pixels,
    word_masks,
)
from current_visibility import project, raster
from PIL import Image

SIZE = 192
PLANES = (
    (-1.8, -1.0, 0.4, 1.0, 2.0),
    (-2.4, -2.4, 2.4, 2.4, 6.0),
    (2.0, -1.6, 3.2, -0.8, 4.0),
)
COLOURS = ((70, 150, 235), (235, 175, 65), (175, 110, 220))


def scene_bounds():
    return [
        np.array([[left, top, z], [right, bottom, z]])
        for left, top, right, bottom, z in PLANES
    ]


def trace(pixels, eye):
    """Independent finite plane-domain intersections for the admitted rays only."""
    pixels = np.array(list(pixels), dtype=np.int32).reshape(-1, 2)
    directions = np.ones((len(pixels), 3))
    directions[:, 0] = 2.0 * (pixels[:, 1] + 0.5) / SIZE - 1.0
    directions[:, 1] = 2.0 * (pixels[:, 0] + 0.5) / SIZE - 1.0
    depth, owner = np.full(len(pixels), np.inf), np.zeros(len(pixels), dtype=np.uint64)
    for index, (left, top, right, bottom, z) in enumerate(PLANES):
        along = z - eye[2]
        points = eye + directions * along
        inside = (
            (points[:, 0] >= left)
            & (points[:, 0] <= right)
            & (points[:, 1] >= top)
            & (points[:, 1] <= bottom)
            & (along <= depth)
            & (along > 0)
        )
        depth[inside], owner[inside] = along, index + 1
    codes = np.zeros((SIZE, SIZE), dtype=np.uint64)
    points = np.zeros((SIZE, SIZE, 3))
    codes[pixels[:, 0], pixels[:, 1]] = owner
    hit = owner != 0
    points[pixels[hit, 0], pixels[hit, 1]] = eye + directions[hit] * depth[hit, None]
    return codes, points


def tile_patches(codes, points):
    """Literal native greedy extent for valid, parallel-plane fixture samples."""
    if not np.isfinite(points[codes != 0]).all():
        raise ValueError("InvalidSamples")
    used = np.zeros(codes.shape, bool)
    result = []
    height, width = codes.shape
    for y in range(height):
        x = 0
        while x < width:
            code = int(codes[y, x])
            if used[y, x] or not code:
                x += 1
                continue

            def compatible(column, row, start_code=code, start_z=points[y, x, 2]):
                return (
                    not used[row, column]
                    and codes[row, column] == start_code
                    and abs(points[row, column, 2] - start_z) <= 1e-6
                )

            right = x + 1
            while right < width and compatible(right, y):
                right += 1
            bottom = y + 1
            while bottom < height and all(
                compatible(column, bottom) for column in range(x, right)
            ):
                bottom += 1
            used[y:bottom, x:right] = True
            result.append((code, x, y, right, bottom))
            x = right
    return result


def patches(codes, points, tiles):
    result = []
    for y, x in sorted(tiles):
        ys, xs = (
            slice(y * 64, min(y * 64 + 64, SIZE)),
            slice(x * 64, min(x * 64 + 64, SIZE)),
        )
        for code, left, top, right, bottom in tile_patches(
            codes[ys, xs], points[ys, xs]
        ):
            result.append(
                (code, left + x * 64, top + y * 64, right + x * 64, bottom + y * 64)
            )
    return result


def emit(retained_patches, eye, *, patch_mode=True):
    """Fine is always emitted in full; only Non-Fine uses the captured patches."""
    left, top, right, bottom, z = PLANES[0]
    models = [
        np.array(
            [[left, top, z], [right, top, z], [right, bottom, z], [left, bottom, z]]
        )
    ]
    colours = [COLOURS[0]]
    if not patch_mode:
        for code in sorted({patch[0] for patch in retained_patches} - {1}):
            left, top, right, bottom, z = PLANES[code - 1]
            models.append(
                np.array(
                    [
                        [left, top, z],
                        [right, top, z],
                        [right, bottom, z],
                        [left, bottom, z],
                    ]
                )
            )
            colours.append(COLOURS[code - 1])
        return models, colours
    for code, left, top, right, bottom in retained_patches:
        if code == 1:
            continue
        rays = np.array(
            [
                [2 * left / SIZE - 1, 2 * top / SIZE - 1, 1],
                [2 * right / SIZE - 1, 2 * top / SIZE - 1, 1],
                [2 * right / SIZE - 1, 2 * bottom / SIZE - 1, 1],
                [2 * left / SIZE - 1, 2 * bottom / SIZE - 1, 1],
            ]
        )
        models.append(eye + rays * (PLANES[code - 1][4] - eye[2]))
        colours.append(COLOURS[code - 1])
    return models, colours


def render(models, colours, eye):
    view = (eye, np.diag([-1.0, 1.0, 1.0]), 360.0, 1280, 720)
    depth, rgb = np.zeros((720, 1280)), np.full((720, 1280, 3), 20, dtype=np.uint8)
    triangles = np.array([[0, 2, 1], [0, 3, 2]])
    for model, colour in zip(models, colours, strict=True):
        projected, z = project(model, view)
        candidate = raster(projected, z, triangles, 1280, 720)
        take = candidate > depth
        depth[take], rgb[take] = candidate[take], colour
    return depth, rgb


def check_emission(before, after, eye, output, pose):
    arms = {}
    for name, patch_mode in (("faces", False), ("patches", True)):
        model_a, colour_a = emit(before, eye, patch_mode=patch_mode)
        model_b, colour_b = emit(after, eye, patch_mode=patch_mode)
        if not np.array_equal(model_a, model_b) or colour_a != colour_b:
            raise ValueError("emitted actual fixture quads differ")
        depth_a, rgb_a = render(model_a, colour_a, eye)
        depth_b, rgb_b = render(model_b, colour_b, eye)
        if not np.array_equal(depth_a, depth_b) or not np.array_equal(rgb_a, rgb_b):
            raise ValueError("fixture output changes depth or colour")
        pixels = int(
            (rgb_a == COLOURS[1]).all(axis=2).sum()
            + (rgb_a == COLOURS[2]).all(axis=2).sum()
        )
        if not pixels:
            raise ValueError("kept Non-Fine surfaces must be visibly rendered")
        for variant, image in (("original", rgb_a), ("admitted", rgb_b)):
            Image.fromarray(image).save(output / f"pose-{pose}-{name}-{variant}.png")
        arms[name] = {
            "emitted_triangles_unchanged": len(model_a) * 2,
            "visible_nonfine_pixels": pixels,
            "changed_depth_pixels": 0,
            "changed_colour_pixels": 0,
        }
    return arms


def run(output):
    rows = []
    for pose, eye in enumerate(
        (np.zeros(3), np.array([0.125, -0.125, 0.0]), np.array([0.25, 0.0, 0.25]))
    ):
        bounds = scene_bounds()
        original = [native_rectangle(box, eye, 2, 1, SIZE) for box in bounds[1:]]
        tiles = occupied_tiles(original, SIZE)
        baseline, _ = word_masks(tiles, [(0, 0, SIZE, SIZE)], SIZE)
        demand = [capture_rectangle(box, eye, 2, 1, SIZE) for box in bounds[1:]]
        admitted, work = word_masks(tiles, demand, SIZE)
        full_codes, full_points = trace(selected_pixels(baseline), eye)
        selected_codes, selected_points = trace(selected_pixels(admitted), eye)
        nonfine = full_codes > 1
        if (
            not np.array_equal(nonfine, selected_codes > 1)
            or not np.array_equal(full_codes[nonfine], selected_codes[nonfine])
            or not np.array_equal(full_points[nonfine], selected_points[nonfine])
        ):
            raise ValueError("admission changes a used Non-Fine sample")
        before, after = (
            patches(full_codes, full_points, tiles),
            patches(selected_codes, selected_points, tiles),
        )
        if [p for p in before if p[0] != 1] != [p for p in after if p[0] != 1]:
            raise ValueError("Fine-only omission changes a used patch rectangle")
        arms = check_emission(before, after, eye, output, pose)
        skipped_fine = int(((full_codes == 1) & (selected_codes == 0)).sum())
        if not skipped_fine or work["selected_rays"] >= work["baseline_rays"]:
            raise ValueError(
                "fixture must exhibit both real work savings and visible kept surfaces"
            )
        # This explicit bad-sample counterexample prevents an unconditional
        # claim about the native global InvalidSamples fallback.
        bad = full_points.copy()
        y, x = np.argwhere((full_codes == 1) & (selected_codes == 0))[0]
        bad[y, x, 0] = np.nan
        try:
            patches(full_codes, bad, tiles)
        except ValueError as error:
            invalid_before = str(error)
        else:
            raise ValueError("injected native-invalid Fine sample must be rejected")
        rows.append(
            {
                "eye": eye.tolist(),
                "mask_work": work,
                "skipped_fine_samples": skipped_fine,
                "used_nonfine_samples": int(nonfine.sum()),
                "used_nonfine_patches": sum(p[0] != 1 for p in before),
                "emission_arms": arms,
                "injected_invalid_fine_sample": {
                    "original": invalid_before,
                    "admitted": "valid",
                    "policy_equivalence": False,
                },
            }
        )
    report = {
        "scope": "Analytic finite planar sources, opaque Fine emitted in full; used Non-Fine samples, native-rule patch extents and emitted quads/RGB/depth unchanged. Fixed cube-grid admission recomputed for each eye; not native data/runtime/device acceptance. RequireShapes/error and InvalidSamples policy still open.",
        "capture_dimension": SIZE,
        "render_dimensions": [1280, 720],
        "poses": rows,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


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
    result = run(target)
    print(json.dumps(result["poses"]), flush=True)
