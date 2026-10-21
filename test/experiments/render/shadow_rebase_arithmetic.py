"""Declared Float32 shader arithmetic with real host-rebased projection coefficients."""

import numpy as np


def packed_view(frame, region, anchor):
    """SunShadowStage::BuildRegions and Cast, host double before Float32 conversion."""
    matrix = np.array(frame["world_matrices"][region]).reshape(4, 4, order="F").copy()
    for row in range(3):
        for axis in range(3):
            matrix[row, 3] -= matrix[row, axis] * frame["pre_view"][axis]
    return matrix.astype(np.float32), (np.asarray(anchor) + frame["pre_view"]).astype(
        np.float32
    )


def shader_points(frame, region, anchor, positions, *, fused=False):
    """Identity local placement plus anchor/PreView shift; declared left-to-right dots."""
    matrix, shift = packed_view(frame, region, anchor)
    local = (np.asarray(positions, dtype=np.float32) + shift).astype(np.float32)
    output = np.zeros((len(local), 3), dtype=np.float32)
    for axis in range(3):
        if fused:
            output = (
                local[:, axis, None].astype(float) * matrix[:3, axis].astype(float)
                + output.astype(float)
            ).astype(np.float32)
        else:
            output = (output + local[:, axis, None] * matrix[:3, axis]).astype(
                np.float32
            )
    return (output + matrix[:3, 3]).astype(np.float32)


def probe(fixture):
    """Deterministic analytic local points, not captured native caster/placement records."""
    axis = np.linspace(-200, 200, 17, dtype=np.float32)
    points = np.stack(np.meshgrid(axis, axis, axis, indexing="ij"), axis=-1).reshape(
        -1, 3
    )
    anchor = fixture["frames"][0]["centre"]
    rows, witnesses = [], []
    for fused in (False, True):
        for region in range(4):
            old = shader_points(
                fixture["frames"][0], region, anchor, points, fused=fused
            )
            previous_matrix = np.asarray(fixture["frames"][0]["world_matrices"][region])
            previous_frame = 0
            depth_changes, maximum, updates = 0, np.zeros(3), 0
            for index, frame in enumerate(fixture["frames"][1:], 1):
                if not fixture["observed"][index - 1]["mask"] & (1 << region):
                    continue
                updates += 1
                current = shader_points(frame, region, anchor, points, fused=fused)
                delta = (
                    np.asarray(frame["world_matrices"][region])[12:15]
                    - previous_matrix[12:15]
                )
                residual = current.astype(float) - old.astype(float) - delta
                changed = np.flatnonzero(residual[:, 2])
                depth_changes += len(changed)
                maximum = np.maximum(maximum, np.max(np.abs(residual), axis=0))
                if len(changed) and not any(
                    w["region"] == region and w["fused"] == fused for w in witnesses
                ):
                    at = int(changed[0])
                    low = float(min(old[at, 2], current[at, 2]))
                    extent_m = min(
                        (256, 1024, 4096, frame["radius_m"])[region], frame["radius_m"]
                    )
                    bias = max(
                        np.float32(3 * np.finfo(np.float32).eps),
                        np.float32(
                            max(0.02, 0.25 * 2 * extent_m / fixture["tile_px"])
                            / (4 * frame["declared_radius_m"])
                        ),
                    )
                    receiver = np.float32(low - bias)
                    threshold = np.float32(receiver + bias)
                    witnesses.append(
                        {
                            "region": region,
                            "fused": fused,
                            "frame": index,
                            "previous_update_frame": previous_frame,
                            "point": points[at].tolist(),
                            "before": old[at].tolist(),
                            "after": current[at].tolist(),
                            "native_region_bias": float(bias),
                            "analytic_receiver_depth": float(receiver),
                            "receiver_threshold": float(threshold),
                            "step_before": int(threshold >= old[at, 2]),
                            "step_after": int(threshold >= current[at, 2]),
                        }
                    )
                old, previous_frame = current, index
                previous_matrix = np.asarray(frame["world_matrices"][region])
            rows.append(
                {
                    "region": region,
                    "fused": fused,
                    "region_updates": updates,
                    "point_frame_depth_differences_on_updates": depth_changes,
                    "max_xy_residual_texels": (
                        maximum[:2] * fixture["tile_px"] / 2
                    ).tolist(),
                    "max_depth_residual": float(maximum[2]),
                }
            )
    return {
        "points": len(points),
        "rows": rows,
        "witnesses": witnesses,
        "scope": "analytic identity placements; declared arithmetic, not observed GPU execution",
        "native_casters_available": False,
        "native_raster_equivalence": None,
    }
