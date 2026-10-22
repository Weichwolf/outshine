"""Shell enclosures from prepared source forms; original mesh data is not an input."""

import itertools
import json
from pathlib import Path

import numpy as np
from beam_intervals import add, dot, exact, sub
from column_roof_patches import bottom_m

CORNERS = np.array(list(itertools.product((0, 1), repeat=3)), bool)


def local_box(surface):
    """Retain the local enclosure used by native SourceEnvelopeBounds before rotation."""
    points = []
    for shape in surface["shapes"]:
        rings = [np.asarray(r, float) for r in (shape["ring"], *shape["holes"])]
        if any(r.ndim != 2 or r.shape[1] != 2 or len(r) < 3 for r in rings):
            raise ValueError("invalid prepared ring")
        xy = np.concatenate(rings)
        bottom = bottom_m(surface, shape)
        top = shape["seat_m"] + shape["foot_m"] + shape["eaves_m"] + shape["rise_m"]
        if (
            not np.isfinite(xy).all()
            or not np.isfinite([bottom, top]).all()
            or bottom > top
        ):
            raise ValueError("invalid prepared shell extent")
        points.extend((np.r_[xy.min(0), bottom], np.r_[xy.max(0), top]))
    if not points:
        raise ValueError("missing prepared shapes")
    return np.nextafter(
        np.stack((np.min(points, axis=0), np.max(points, axis=0))),
        [[-np.inf], [np.inf]],
    )


def build(parent):
    """Once during enrichment; material fields are decoded but no triangles are emitted."""
    count = len(parent.plans["records"])
    arrays = {
        "sources": np.array(
            [(i, *r["source_id"]) for i, r in enumerate(parent.plans["records"])],
            dtype=[("ordinal", "<u4"), ("kind", "u1"), ("id", "<u8")],
        ),
        "cells": parent.cells.copy(),
        "frames": np.zeros((count, 4, 3)),
        "local_bounds": np.zeros((count, 2, 3)),
        "padding": np.zeros(count),
        "state": np.zeros(count, np.uint8),
    }
    needs, shapes = [], 0
    for cell, ordinals, surfaces in parent.iter_forms():
        for ordinal, surface in zip(ordinals, surfaces, strict=True):
            row = int(ordinal)
            if surface is None:
                needs.append(
                    {
                        "ordinal": row,
                        "cell": int(cell),
                        "reason": "original form unavailable",
                    }
                )
                continue
            summary = parent.plans["summaries"][row]
            error = summary["shell_error_m"]
            if error is None or not np.isfinite(error) or error < 0:
                arrays["state"][row] = 2
                needs.append(
                    {
                        "ordinal": row,
                        "cell": int(cell),
                        "reason": "native shell-error contract unavailable",
                    }
                )
                continue
            try:
                bounds = local_box(surface)
                if (
                    np.shape(summary["frame"]) != (4, 3)
                    or not np.isfinite(summary["frame"]).all()
                ):
                    raise ValueError("invalid prepared source frame")
            except ValueError as failure:
                arrays["state"][row] = 3
                needs.append(
                    {"ordinal": row, "cell": int(cell), "reason": str(failure)}
                )
                continue
            arrays["frames"][row] = summary["frame"]
            (
                arrays["local_bounds"][row],
                arrays["padding"][row],
                arrays["state"][row],
            ) = bounds, error, 1
            shapes += len(surface["shapes"])
    return arrays, {
        "source_records": count,
        "shape_records": shapes,
        "available": int((arrays["state"] == 1).sum()),
        "needs_original": needs,
        "form_reads": parent.form_reads,
        "form_bytes": parent.form_bytes,
        "native_mesh_decodes": 0,
        "native_mesh_values_consumed": 0,
        "emitted_triangles": 0,
    }


def affine_ranges(points, frame):
    """Outward affine image for interval rows; frame stores origin then three axes."""
    low, high = points
    values = dot(
        (low[:, None, :], high[:, None, :]), exact(np.asarray(frame[1:]).T[None])
    )
    return add(values, exact(np.asarray(frame[0])))


def world_corners(product, ordinal, *, lod="shell"):
    if lod != "shell":
        return None
    arrays = product["arrays"]
    at = int(np.searchsorted(arrays["sources"]["ordinal"], ordinal))
    if (
        at >= len(arrays["sources"])
        or arrays["sources"]["ordinal"][at] != ordinal
        or arrays["state"][at] != 1
    ):
        return None
    bounds = arrays["local_bounds"][at]
    corners = np.where(CORNERS, bounds[1], bounds[0])
    low, high = affine_ranges(exact(corners), arrays["frames"][at])
    error = exact(arrays["padding"][at])
    return sub(exact(low), error)[0], add(exact(high), error)[1]


def store(arrays, parent, path):
    path = Path(path)
    path.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(path / "support.npz", **arrays)
    header = {
        "format": "prepared-shell-support-1",
        "identity": {"parent": parent.parent, "archive_sha256": parent.archive_sha256},
        "anchor_ecef": list(parent.plans["anchor_ecef"]),
        "lod": "shell",
        "sources": len(arrays["sources"]),
        "arrays": {k: a.nbytes for k, a in arrays.items()},
        "stored_array_bytes": sum(a.nbytes for a in arrays.values()),
        "additional_enclosure_bytes": arrays["local_bounds"].nbytes
        + arrays["state"].nbytes,
        "container_bytes": (path / "support.npz").stat().st_size,
        "scope": "experimental prepared Shell support; frames/IDs/cells/native padding already exist in PSB1. Not a native cache-format proposal; Fine/missing support retain originals",
    }
    (path / "header.json").write_text(json.dumps(header, indent=2) + "\n")
    return header


def load_product(path, identity):
    path = Path(path)
    header = json.loads((path / "header.json").read_text())
    if header["format"] != "prepared-shell-support-1" or header["identity"] != identity:
        raise ValueError("prepared-support source identity mismatch")
    with np.load(path / "support.npz", allow_pickle=False) as archive:
        arrays = {k: archive[k] for k in archive.files}
    count = header["sources"]
    if len(arrays["sources"]) != count or np.any(
        np.diff(arrays["sources"]["ordinal"].astype(np.int64)) <= 0
    ):
        raise ValueError("prepared-support source order mismatch")
    for key, shape in (
        ("frames", (count, 4, 3)),
        ("local_bounds", (count, 2, 3)),
        ("padding", (count,)),
        ("state", (count,)),
        ("cells", (count,)),
    ):
        if arrays[key].shape != shape or not np.isfinite(arrays[key]).all():
            raise ValueError("invalid prepared-support array: " + key)
    bounds = arrays["local_bounds"]
    if np.any(bounds[:, 0] > bounds[:, 1]) or np.any(arrays["padding"] < 0):
        raise ValueError("invalid prepared-support bounds/padding")
    return {"header": header, "arrays": arrays}
