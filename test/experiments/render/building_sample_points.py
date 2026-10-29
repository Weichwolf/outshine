"""Finite same-captured-cell native response; never original visibility authority."""

from itertools import product

import numpy as np
from building_sample_inputs import crop_view
from field_view_geometry import distribution
from replay_building_radiance import compare

OFFSETS = np.asarray(list(product((-1, 0, 1), repeat=2)), dtype=np.int64)


def capture_channels(manifest, arrays, pixels, mask):
    crop, raster = np.asarray(manifest["crop"]), np.asarray(manifest["raster"])
    low, high = crop[:2] - (raster - pixels) // 2, crop[2:] - (raster - pixels) // 2
    if np.any(low < 0) or np.any(high > pixels):
        raise ValueError("Original capture crop must fit the registered sample grid")
    depth = np.zeros((pixels, pixels), np.float32)
    normal = np.zeros((pixels, pixels, 3), np.float32)
    target = np.s_[low[1] : high[1], low[0] : high[0]]
    depth[target] = arrays["case0_samples_depth"][:, :, 0]
    normal[target] = arrays["case0_samples_normal"][:, :, :3]
    if (
        not np.isfinite(depth).all()
        or np.any((depth < 0) | (depth > 1))
        or not np.array_equal(depth > 0, mask)
        or not np.isfinite(normal).all()
        or np.any(np.linalg.norm(normal[mask], axis=1) == 0)
    ):
        raise ValueError("Actual capture depth/normal must match its opaque point mask")
    return depth, normal


def pixel_points(view, depth, *, points_most):
    if (
        depth.shape != (view.height, view.width)
        or not np.isfinite(depth).all()
        or np.any((depth < 0) | (depth > 1))
    ):
        raise ValueError("Finite native perspective base-frame depth required")
    y, x = np.nonzero(depth > 0)
    if not 0 < len(x) <= points_most:
        raise ValueError("Bounded nonempty native sample-point query required")
    basis = np.asarray(view.basis)
    ray = (
        basis[2]
        + ((x + 0.5 - view.width / 2) / view.focal_px)[:, None] * basis[0]
        - ((y + 0.5 - view.height / 2) / view.focal_px)[:, None] * basis[1]
    )
    # The frozen native Draw sets near=.1 and uses infinite reversed perspective Z.
    forward = float(np.float32(0.1)) / depth[y, x].astype(float)
    return (
        np.column_stack((x, y)),
        np.asarray(view.eye) + ray * forward[:, None],
        forward,
    )


def match_cells(
    grid,
    view,
    mask,
    source_depth,
    source_normal,
    depth,
    normal,
    *,
    operations=64,
    points_most=16384,
):
    if operations not in (32, 64, 128):
        raise ValueError("Declared Float32 engineering-roundoff sensitivity required")
    if (
        mask.dtype != bool
        or mask.ndim != 2
        or mask.shape[0] != mask.shape[1]
        or not 3 <= len(mask) <= 512
    ):
        raise ValueError("Bounded square registered source-point grid required")
    if source_depth.shape != mask.shape or source_normal.shape != (*mask.shape, 3):
        raise ValueError("Matching actual source depth/normal channels required")
    if normal.shape != (*depth.shape, 4) or not np.isfinite(normal).all():
        raise ValueError("Matching actual native shading-normal output required")
    pixels, point, forward = pixel_points(view, depth, points_most=points_most)
    x, y = pixels.T
    relative, size = point - grid.centre, len(mask)
    right, up = grid.axes()
    uv = np.column_stack((relative @ right, -relative @ up))
    uv = (uv / (2 * grid.half_extent_m) + 0.5) * size
    height = relative @ np.asarray(grid.toward_eye)
    # An explicit engineering check, not a certified backend error bound. Compare
    # 32/64/128 operation budgets and retain unresolved/ambiguous points as such.
    world_guard = (
        operations
        * np.finfo(np.float32).eps
        * np.maximum(1, np.maximum(abs(point).max(1), forward))
    )
    uv_guard = world_guard / (2 * grid.half_extent_m / size)
    candidates = np.floor(np.clip(uv, -2, size + 1)).astype(np.int64)[:, None] + OFFSETS
    inside = ((candidates >= 0) & (candidates < size)).all(2)
    overlaps = (
        (candidates <= uv[:, None] + uv_guard[:, None, None])
        & (candidates + 1 >= uv[:, None] - uv_guard[:, None, None])
    ).all(2)
    ix, iy = np.clip(candidates, 0, size - 1).transpose(2, 0, 1)
    residual = abs(
        height[:, None] - (4 * source_depth[iy, ix] - 2) * grid.half_extent_m
    )
    normal_equal = (normal[y, x, :3][:, None] == source_normal[iy, ix]).all(2)
    matched = (
        inside
        & overlaps
        & mask[iy, ix]
        & (residual <= world_guard[:, None])
        & normal_equal
    )
    counts = matched.sum(1)
    choice, rows = np.argmax(matched, axis=1), np.arange(len(pixels))
    chosen = np.column_stack((ix[rows, choice], iy[rows, choice]))
    margin = np.minimum(uv - chosen, chosen + 1 - uv).min(1)
    resolved = (counts == 1) & (uv_guard < 0.25)
    interior = resolved & (margin >= 0.25)
    toward = np.asarray(view.eye) - point
    toward /= np.linalg.norm(toward, axis=1)[:, None]
    angle = np.degrees(np.arccos(np.clip(toward @ np.asarray(grid.toward_eye), -1, 1)))
    return {
        "pixels_xy": pixels,
        "source_xy": chosen,
        "source_uv": uv,
        "candidate_counts": counts,
        "unique_geometry": resolved,
        "interior_texture": interior,
        "source_cell_margin": margin,
        "world_height_residual_m": residual[rows, choice],
        "world_guard_m": world_guard,
        "view_angle_deg": angle,
        "engineering_operations": operations,
    }


def response(source, query, mapping, mask):
    if not mask.any():
        return None
    sx, sy = mapping["source_xy"][mask].T
    x, y = mapping["pixels_xy"][mask].T
    first, second = source[sy, sx].astype(float), query[y, x].astype(float)
    if first[:, :3].sum() > 0:
        result = compare(first, second)
    else:
        result = {
            "rgb_relative_l1": None,
            "rgb_absolute_l1": float(abs(first[:, :3] - second[:, :3]).sum()),
            "radiance_ratio": None,
            "alpha_relative_l1": float(
                abs(first[:, 3] - second[:, 3]).sum() / first[:, 3].sum()
            ),
            "coverage_ratio": float(second[:, 3].sum() / first[:, 3].sum()),
            "reason": "zero_capture_radiance_relative_error_undefined",
        }
    result.update(
        points=len(first),
        maximum_rgb_absolute_delta=float(abs(first[:, :3] - second[:, :3]).max()),
        source_cell_margin=distribution(mapping["source_cell_margin"][mask]),
        view_angle_deg=distribution(mapping["view_angle_deg"][mask]),
        depth_model_residual_m=distribution(mapping["world_height_residual_m"][mask]),
    )
    return result


def summarize(source, query, mapping):
    counts, resolved = mapping["candidate_counts"], mapping["unique_geometry"]
    interior = mapping["interior_texture"]
    return {
        "native_base_sample_hits": len(counts),
        "candidate_cell_tests": len(counts) * len(OFFSETS),
        "unique_geometry_matches": int(resolved.sum()),
        "ambiguous_geometry_matches": int((counts > 1).sum()),
        "unmatched_geometry": int((counts == 0).sum()),
        "unresolved_numeric_precision": int(((counts == 1) & ~resolved).sum()),
        "quarter_cell_interior_points": int(interior.sum()),
        "engineering_operations": mapping["engineering_operations"],
        "geometry_matched_total_response": response(source, query, mapping, resolved),
        "interior_same_cell_response": response(source, query, mapping, interior),
        "scope": "Same native stored-sample model; correspondence selected only by depth/normal, never RGB. A declared engineering Float32 neighbourhood is not a backend certificate. Quarter-cell interiors isolate a finite material-sample cohort; boundary geometry matches may still have texture-nearest ambiguity. Source/material sampling, shader arithmetic and viewpoint can all affect total response. No original visibility, whole-image BRDF, continuous pose or World/depth authority.",
    }


def analyse_points(primary, canonical, grid, source):
    manifest, arrays = primary
    capture_depth, capture_normal = capture_channels(
        *canonical, len(source), source[:, :, 3] > 0
    )
    rows, saved = [], {}
    for index, case in enumerate(manifest["cases"]):
        if "capture" in case["query"]:
            raise ValueError("Perspective native Draw base-frame controls required")
        view = crop_view(manifest, case["query"])
        depth, normal, colour = (
            arrays[f"case{index}_samples_{suffix}"]
            for suffix in ("depth", "normal", "linear")
        )
        if not np.array_equal(depth[:, :, 0] > 0, colour[:, :, 3] > 0):
            raise ValueError("Actual native sample depth/colour hit masks differ")
        variants = {
            operations: match_cells(
                grid,
                view,
                source[:, :, 3] > 0,
                capture_depth,
                capture_normal,
                depth[:, :, 0],
                normal,
                operations=operations,
            )
            for operations in (32, 64, 128)
        }
        base = variants[64]
        stable = base["unique_geometry"].copy()
        for value in variants.values():
            stable &= value["unique_geometry"] & (
                base["source_xy"] == value["source_xy"]
            ).all(1)
        base["stable_geometry"] = stable
        base["interior_texture"] &= stable
        row = dict(summarize(source, colour, base), query=case["query"])
        row["roundoff_unstable_geometry_matches"] = int(
            (base["unique_geometry"] & ~stable).sum()
        )
        row["roundoff_sensitivity"] = []
        for operations, value in variants.items():
            common = base["unique_geometry"] & value["unique_geometry"]
            row["roundoff_sensitivity"].append(
                {
                    "operations": operations,
                    "unique_geometry_matches": int(value["unique_geometry"].sum()),
                    "quarter_cell_interior_points": int(
                        value["interior_texture"].sum()
                    ),
                    "changed_source_cells_on_common_matches": int(
                        np.count_nonzero(
                            np.any(
                                base["source_xy"][common] != value["source_xy"][common],
                                axis=1,
                            )
                        )
                    ),
                }
            )
        row["candidate_cell_tests_all_budgets"] = row["candidate_cell_tests"] * len(
            variants
        )
        rows.append(row)
        for name, value in base.items():
            if isinstance(value, np.ndarray):
                saved[f"case{index}_{name}"] = value
    return rows, saved
