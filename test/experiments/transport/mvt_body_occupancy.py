"""Existing affine body/headroom products from actual indexed MVT cap pairs."""

import math

import numpy as np
from outshine_ref.contracts import identifier
from stair_occupancy_inputs import volume


def _inputs(data, source_ids, clearances, owner_id, method, max_volumes):
    identifier(owner_id)
    if method not in ("faces", "sources"):
        raise ValueError("known body-occupancy preparation method required")
    if type(max_volumes) is not int or not 1 <= max_volumes <= 8192:
        raise ValueError("bounded positive occupancy-volume budget required")
    if not 1 <= len(source_ids) <= 64 or len(set(source_ids)) != len(source_ids):
        raise ValueError("bounded unique source order required")
    for key in source_ids:
        identifier(key)
    if set(clearances) != set(source_ids) or any(
        not math.isfinite(value) or not 0 <= value <= 32
        for value in clearances.values()
    ):
        raise ValueError("explicit finite bounded clearance for every source required")
    vertices = np.asarray(data["body_vertices"])
    top, bottom, masks = (
        np.asarray(data[key]) for key in ("top_faces", "bottom_faces", "face_sources")
    )
    if (
        vertices.ndim != 2
        or vertices.shape[1] != 3
        or vertices.dtype != np.dtype("float64")
        or not 1 <= len(vertices) <= 16384
        or not np.isfinite(vertices).all()
        or abs(vertices).max() > 1e6
        or not 1 <= len(top) <= 4096
        or top.shape != (len(top), 3)
        or bottom.shape != top.shape
        or masks.shape != (len(top),)
        or any(array.dtype.kind not in "iu" for array in (top, bottom, masks))
        or min(top.min(), bottom.min()) < 0
        or max(top.max(), bottom.max()) >= len(vertices)
    ):
        raise ValueError(
            "bounded finite paired indexed cap buffers and source masks required"
        )
    flags = [int(mask) for mask in masks]
    if any(mask <= 0 or mask >> len(source_ids) for mask in flags):
        raise ValueError("every cap must retain known nonempty source ownership")
    count = len(top) if method == "faces" else sum(mask.bit_count() for mask in flags)
    if count > max_volumes:
        raise ValueError("occupancy-volume budget exhausted before geometry work")
    return vertices, top, bottom, flags


def body_occupancy(
    data, source_ids, clearances, *, owner_id, method="faces", max_volumes=8192
):
    """Prepare existing immutable AffineOccupancy rows, only during asset generation.

    Each actual top/bottom cap pair defines a vertical column. Shared physical
    caps use the maximum of their source clearances; all contributing source
    IDs and clearance requirements remain in the records. The source-expanded
    reference produces the same occupied space through separate duplicate rows.
    No reference-height envelope, inferred body size, footprint enlargement or
    automatic profile fallback substitutes for actual indexed caps.
    """
    vertices, top, bottom, masks = _inputs(
        data, source_ids, clearances, owner_id, method, max_volumes
    )
    source_ids = tuple(source_ids)
    rows, origins, records, errors = [], [], [], []
    for face, mask in enumerate(masks):
        keys = tuple(key for i, key in enumerate(source_ids) if mask & (1 << i))
        groups = (keys,) if method == "faces" else [(key,) for key in keys]
        for group in groups:
            required = max(float(clearances[key]) for key in group)
            suffix = (
                ""
                if method == "faces"
                else ":source:" + str(source_ids.index(group[0]))
            )
            row, error = volume(
                f"{owner_id}:face:{face}{suffix}",
                vertices[bottom[face]],
                vertices[top[face]],
                required,
            )
            rows.append(row)
            origins.append(group)
            records.append(
                {
                    "face": face,
                    "source_ids": group,
                    "source_clearances_m": {
                        key: float(clearances[key]) for key in group
                    },
                    "clearance_m": required,
                }
            )
            errors.append(error)
    return (
        tuple(rows),
        tuple(origins),
        records,
        {
            "actual_cap_pairs": len(top),
            "source_face_memberships": sum(mask.bit_count() for mask in masks),
            "emitted_volumes": len(rows),
            "plane_fits": 2 * len(rows),
            "indexed_cap_coordinate_bytes": len(rows)
            * 2
            * 3
            * 3
            * vertices.dtype.itemsize,
            "maximum_outward_height_padding_m": max(errors, default=0),
            "unused_sources": [
                {"source_id": key, "reason": "not_owned_by_any_supplied_cap"}
                for i, key in enumerate(source_ids)
                if not any(mask & (1 << i) for mask in masks)
            ],
            "scope": "Reuse of existing exact-corner/outward-rounded affine cap construction. Identified cap-array bytes are not complete allocations or GEOS copy traffic. Positive-volume/height-aware polygon queries keep their existing numeric limits. No gait, outside-port or native/world admission; no hot-path library calls.",
        },
    )
