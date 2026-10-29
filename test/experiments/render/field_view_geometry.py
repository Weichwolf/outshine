"""Separate centre-sample depth/normal diagnostics; never main-depth admission."""

import numpy as np
from field_view_sampling import source_corners


def distribution(values):
    return (
        dict(
            zip(
                ("p50", "p95", "maximum"),
                map(float, np.quantile(values, [0.5, 0.95, 1])),
            )
        )
        if len(values)
        else None
    )


def compare_geometry(atlas, view, depth, normal):
    if depth.shape != (view.height, view.width) or normal.shape != (*depth.shape, 4):
        raise ValueError("Full-resolution base depth and shading normal required")
    hit = depth != 0
    if (
        not np.isfinite(depth).all()
        or np.any((depth < 0) | (depth > 1))
        or not np.isfinite(normal[hit]).all()
        or np.any(np.linalg.norm(normal[hit, :3], axis=1) == 0)
    ):
        raise ValueError("Finite native base-frame readbacks required")
    y, x = np.nonzero(hit)
    grid, n = atlas["grid"], atlas["pixels"]
    # source_corners accepts continuous screen positions. Its first corner here
    # is exactly the base-frame pixel centre, not a supersampled colour average.
    uv = source_corners(
        grid, view, grid.axes(), np.column_stack((x + 0.5, y + 0.5)), (n, n)
    )[:, 0]
    inside = np.all((uv >= 0) & (uv < n), axis=1)
    ix, iy = np.clip(np.floor(uv).astype(int), 0, n - 1).T
    texels = atlas["texels"][iy, ix]
    covered = inside & (texels["surface"] > 0)
    basis, eye = np.asarray(view.basis), np.asarray(view.eye)
    direction = (
        basis[2]
        + (x + 0.5 - view.width / 2)[:, None] * basis[0] / view.focal_px
        - (y + 0.5 - view.height / 2)[:, None] * basis[1] / view.focal_px
    )
    distance = (np.asarray(grid.centre) - eye) @ np.asarray(grid.toward_eye)
    plane_distance = distance / (direction @ np.asarray(grid.toward_eye))
    original_distance = np.float32(0.1).astype(float) / depth[y, x].astype(float)
    right, up = grid.axes()
    # Native orthographic reversed depth: near=H, far=5H, eye=C+3H*d.
    # Its first hit has signed height h=(4*depth-2)*H along toward_eye.
    point = (
        np.asarray(grid.centre)
        + ((ix + 0.5) / n * 2 - 1)[:, None] * grid.half_extent_m * right
        + (1 - (iy + 0.5) / n * 2)[:, None] * grid.half_extent_m * up
        + (4 * texels["depth"].astype(float) - 2)[:, None]
        * grid.half_extent_m
        * np.asarray(grid.toward_eye)
    )
    field_distance = (point - eye) @ basis[2]
    original_normal = normal[y[covered], x[covered], :3].astype(float)
    field_normal = texels["normal"][covered].astype(float)
    original_normal /= np.linalg.norm(original_normal, axis=1)[:, None]
    field_normal /= np.linalg.norm(field_normal, axis=1)[:, None]
    angle = np.degrees(
        np.arccos(np.clip(np.sum(original_normal * field_normal, axis=1), -1, 1))
    )
    return {
        "original_centre_hits": int(hit.sum()),
        "original_hits_without_captured_surface": int((~covered).sum()),
        "matched_centre_samples": int(covered.sum()),
        "plane_forward_depth_absolute_m": distribution(
            abs(plane_distance - original_distance)
        ),
        "first_layer_forward_depth_absolute_m": distribution(
            abs(field_distance[covered] - original_distance[covered])
        ),
        "first_layer_normal_angle_deg": distribution(angle),
        "scope": "Only original-covered base-frame pixel centres; misses are explicit. Nearest first-hit capture texel versus native centre depth and half-target shading normal. Flat filtered radiance has no main-depth/normal, external occlusion or shadow authority.",
    }
