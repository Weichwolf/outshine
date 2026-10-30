"""Whole disjoint ground modules extend a retained raised transport snapshot."""

import json
from dataclasses import replace
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from rail_occupancy_inputs import bound_state
from stair_local_replay import append_verified


def xy_bounds(vertices, triangles=None):
    xy = vertices[:, :2] if triangles is None else vertices[triangles, :2]
    if not np.isfinite(xy).all() or np.max(abs(xy)) > 1e6:
        raise ValueError("finite bounded local transport XY required")
    rounded = xy.astype("f4").astype(float)
    lower = np.minimum(xy, rounded).min(axis=-2)
    upper = np.maximum(xy, rounded).max(axis=-2)
    return np.concatenate(
        (np.nextafter(lower, -np.inf), np.nextafter(upper, np.inf)), axis=-1
    )


def disjoint(box, occupied):
    return not np.any(
        np.all(occupied[:, :2] <= box[2:], axis=1)
        & np.all(box[:2] <= occupied[:, 2:], axis=1)
    )


def extend(products, paths=()):
    paths = tuple(Path(p).resolve() for p in paths)
    if not paths:
        return products, []
    if products.state_inputs is None:
        raise ValueError("explicit verified raised base lineage required")
    resident, prior = products.state_inputs
    combined = (*prior, *paths)
    if len(combined) > 32 or len(set(combined)) != len(combined):
        raise ValueError("bounded unique ordered transport extensions required")
    state = bound_state(products, resident, prior)
    root = json.loads((resident / "resident.json").read_text())
    required = {"frame", "origin_lat_lon", "support_sha256"}
    if (
        not required <= root.keys()
        or not required <= state[4].keys()
        or any(state[4].get(key) != root.get(key) for key in required)
        or state[0].patches[0].frame.identifier != root.get("frame")
    ):
        raise ValueError(
            "same verified ground/raised frame, origin and supports required"
        )
    occupied = np.vstack(
        [
            xy_bounds(mesh.vertices, mesh.triangles)
            for mesh in (products.deck, *products.supports)
        ]
    )
    elevated = {row["source_id"] for row in products.rows}
    proofs = []
    for path in paths:
        report = json.loads((path / "comparison.json").read_text())
        if not report.get("valid_local_module"):
            raise ValueError("whole independently admitted ground extension required")
        candidate = append_verified(state, path)
        if (
            len(candidate[1]) != len(state[1]) + 1
            or any(a is not b for a, b in zip(state[1], candidate[1][:-1], strict=True))
            or candidate[2] is not state[2]
            or candidate[3] is not state[3]
        ):
            raise ValueError("ground extension must retain every old product")
        asset = candidate[1][-1]
        if (
            not asset.data["ground"].all()
            or set(asset.source_ids) & elevated
            or any(
                key not in products.originals
                or products.originals[key]["properties"].get("brunnel")
                or products.originals[key]["properties"].get("indoor")
                or products.originals[key]["properties"].get("layer") not in (None, 0)
                for key in asset.source_ids
            )
        ):
            raise ValueError("known ground roles distinct from raised sources required")
        body = xy_bounds(asset.data["body_vertices"])
        region = xy_bounds(candidate[0].patches[-1].mesh.vertices)
        if not disjoint(body, occupied) or not disjoint(region, occupied):
            raise ValueError(
                "overlapping raised region requires separate whole admission"
            )
        proofs.append(
            {
                "path": str(path),
                "report_sha256": digest(path / "comparison.json"),
                "receipt_sha256": digest(path / "verification.json"),
                "source_ids": list(asset.source_ids),
                "body_xy_bounds_double_and_float32": body.tolist(),
                "changed_region_xy_bounds_double_and_float32": region.tolist(),
                "raised_face_boxes": len(occupied),
                "scope": "Both-precision whole body/changed-region XY envelopes strictly separated from every retained raised body face. Not a vertical underpass or Source/World admission.",
            }
        )
        state = candidate
    return replace(products, state=state, state_inputs=(resident, combined)), proofs
