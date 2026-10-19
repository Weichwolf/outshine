"""Count sampled nonempty cells before attempting a subpixel far-voxel product.

This is an occupancy/cost preflight on original Fine ray hits. It fits no shader,
constructs no complete voxel asset and grants no coverage/appearance admission.
"""

import numpy as np
from far_product_cut import projection_error


def voxel_preflight(positions, eye, hits, *, extent_multiple, focal_px, width, height):
    low, high = positions.min(0).astype(float), positions.max(0).astype(float)
    # Common pitch for all diagnostic orbit angles and 0/32 m translations at
    # this distance level, including the existing 64 m eye-reuse cube.
    distance = (
        extent_multiple * np.ptp(positions, axis=0).max() * np.sqrt(1.09)
        - np.linalg.norm(high - low) / 2
        - 32
        - 64 * np.sqrt(3)
    )
    if distance <= 0:
        return {"eligible": False, "reason": "observer reuse region too close"}
    factor = np.linalg.norm([1, width / (2 * focal_px), height / (2 * focal_px)])
    diameter = 0.5 * distance / (factor * (focal_px * factor + 0.5))
    pitch = diameter / np.sqrt(3)
    box = np.stack((low - pitch, high + pitch))
    observer = np.stack((eye - 64, eye + 64))
    bound = projection_error(
        box,
        observer,
        displacement_m=diameter,
        focal_px=focal_px,
        width=width,
        height=height,
    )
    if not np.isfinite(bound) or bound > 1:
        return {"eligible": False, "reason": "voxel point-pair projection unproved"}
    coordinate = np.floor((hits.astype(float) - low) / pitch).astype(np.int64)
    occupied = np.unique(coordinate, axis=0)
    return {
        "eligible": True,
        "pitch_m": float(pitch),
        "max_voxel_point_pair_px_bound": float(bound),
        "sampled_source_hits": len(hits),
        "sampled_nonempty_cells": len(occupied),
        "position_u16_per_axis_possible": bool(
            (np.ceil((high - low) / pitch) <= 65535).all()
        ),
        "optimistic_position_normal_colour_bytes": len(occupied) * 14,
        "accounting_assumption": "6-byte position, 4-byte normal, 4-byte colour per observed cell; excludes shader/material/UV/source membership/opacity/hierarchy/fallback data, not a measured or minimal possible layout.",
        "quantised_coordinate_array_bytes": coordinate.nbytes,
        "unique_coordinate_array_bytes": occupied.nbytes,
        "work_scope": "One grid-coordinate conversion per original Fine hit plus library uniqueness; internal allocations/sort work not counted. Observed cells are not a complete voxelisation or a globally minimal rendering.",
        "validity_scope": "Bound holds only between points in one voxel, not between reconstructed surfaces or their lighting. No opacity, material, near/far or device admission.",
    }
