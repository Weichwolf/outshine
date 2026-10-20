"""Shared perspective vector field: analytic surfaces and original trim boundaries."""

import json
from pathlib import Path

import numpy as np
from attributed_prisms import native_inputs
from projective_field_bins import build


def frame(source, up):
    low, high = (
        source["positions"].min(0).astype(float),
        source["positions"].max(0).astype(float),
    )
    extent = high - low
    axis = int(np.argmin(extent))
    forward = np.roll(np.array([1.0, 0.171, 0.317]), axis)
    forward /= np.linalg.norm(forward)
    eye = (low + high) / 2 - forward * (16 * max(float(extent.max()), 1.0))
    right = np.cross(up, forward)
    if np.linalg.norm(right) < 1e-8:
        right = np.cross(np.eye(3)[np.argmin(abs(forward))], forward)
    right /= np.linalg.norm(right)
    basis = np.stack((right, np.cross(forward, right), forward))
    if not np.isfinite(basis).all():
        raise ValueError("unsupported source-only projection frame")
    return eye, basis


def sparse(table):
    present = np.any(table != 0, axis=(1, 2))
    if not present.any():
        return np.empty((0, *table.shape[1:]), table.dtype), np.empty(0, np.uint16)
    dtype = np.uint16 if int(present.sum()) < 65535 else np.uint32
    index = np.full(len(table), np.iinfo(dtype).max, dtype)
    index[present] = np.arange(int(present.sum()), dtype=dtype)
    return table[present], index


def encode(source, up, profile, *, dtype=np.float32):
    if dtype not in (np.float32, np.float64):
        raise ValueError(
            "explicit Float32 product or Float64 analytic control required"
        )
    faces = np.arange(len(source["indices"]))
    if not 0 < len(faces) <= 131072:
        raise ValueError("bounded nonempty source surface required")
    eye, basis = frame(source, up)
    indices = source["indices"][faces]
    local = (source["positions"][indices].astype(float) - eye) @ basis.T
    if not np.isfinite(local).all() or np.min(local[:, :, 2]) <= 0.1:
        raise ValueError(
            "projection crosses near plane: original/finer products required"
        )
    focal = float(profile["focal_px"])
    if (
        not np.isfinite(focal)
        or focal <= 0
        or min(profile["width"], profile["height"]) <= 0
    ):
        raise ValueError("positive finite projection profile required")
    xy = focal * local[:, :, :2] / local[:, :, 2, None]
    footprint = xy.astype(dtype)
    e, f = (
        footprint[:, 1].astype(float) - footprint[:, 0],
        footprint[:, 2].astype(float) - footprint[:, 0],
    )
    area = e[:, 0] * f[:, 1] - e[:, 1] * f[:, 0]
    if np.any(area == 0) or not np.isfinite(footprint).all():
        raise ValueError(
            "degenerate projected boundary: original/finer products required"
        )
    original_e, original_f = xy[:, 1] - xy[:, 0], xy[:, 2] - xy[:, 0]
    original_area = (
        original_e[:, 0] * original_f[:, 1] - original_e[:, 1] * original_f[:, 0]
    )
    if np.any(area * original_area <= 0):
        raise ValueError("quantised footprint changes surface orientation")
    if not np.all(source["normal"][indices] == source["normal"][indices][:, :1]):
        raise ValueError("profile requires flat original shading normals")
    inputs = native_inputs(source)
    q = 1 / local[:, :, 2, None]
    metres, metre_index = sparse(source["metres"][indices].astype(dtype))
    colour, colour_index = sparse((source["colour"][indices] - 1).astype(dtype))
    lineage, owner = np.unique(
        source["binding"][faces, :5], axis=0, return_inverse=True
    )
    arrays = {
        "footprint": footprint,
        "inverse_depth": q[:, :, 0].astype(dtype),
        "coordinates": inputs["uv"][indices].astype(dtype),
        "normal": source["normal"][indices[:, 0]],
        "code": inputs["flat_codes"][faces],
        "metres": metres,
        "metre_index": metre_index,
        "colour_delta": colour,
        "colour_index": colour_index,
        "lineage": lineage,
        "lineage_index": owner.astype(
            np.uint16 if len(lineage) <= 65536 else np.uint32
        ),
        "original_face": faces.astype(np.uint32),
        "native_face_tag": source["binding"][faces, 5],
        "eye": eye,
        "basis": basis,
        "focal_px": np.array(focal),
    }
    arrays.update(build(footprint.astype(float)))
    q_at_vertices = arrays["inverse_depth"].astype(float)
    if np.any(q_at_vertices <= 0):
        raise ValueError("quantised inverse depth crosses a pole")
    z = 1 / q_at_vertices
    arrays["depth_range"] = np.array([z.min(), z.max()])
    world = (
        eye
        + np.concatenate(
            (footprint.astype(float) / focal * z[:, :, None], z[:, :, None]), axis=2
        )
        @ basis
    )
    arrays["world_bounds"] = np.stack((world.min(axis=(0, 1)), world.max(axis=(0, 1))))
    # Outward depth support for the decoded programs, including floating arithmetic.
    guard = 32 * np.spacing(float(z.max()))
    arrays["depth_range"] += [-guard, guard]
    work = {
        "input_triangles": len(faces),
        "discarded_triangles": 0,
        "triangle_gather_bytes": local.nbytes,
        "projected_vertices": 3 * len(faces),
        "bin_references": len(arrays["bin_faces"]),
        "frame": "source bounds only; no camera or viewing-domain pruning",
    }
    return arrays, work


def cook(source, up, profile, path, provenance, *, dtype=np.float32):
    arrays, work = encode(source, up, profile, dtype=dtype)
    path = Path(path)
    path.mkdir(parents=True, exist_ok=False)
    np.savez(path / "field.npz", **arrays)
    represented = np.unique(arrays["lineage"][:, 1])
    missing = np.setdiff1d(source["binding"][:, 1], represented)
    header = {
        "format": "outshine-projective-vector-field-v1",
        "provenance": provenance,
        "profile": profile,
        "coordinate_bounds": [[-1e7] * 3, [1e7] * 3],
        "world_bounds": arrays["world_bounds"].tolist(),
        "programs": len(arrays["original_face"]),
        "represented_sources": len(represented),
        "omitted_sources": [
            {
                "source": int(s),
                "reason": "no represented source faces",
            }
            for s in missing
        ],
        "array_bytes": {k: a.nbytes for k, a in arrays.items()},
        "total_array_bytes": sum(a.nbytes for a in arrays.values()),
        "container_bytes": (path / "field.npz").stat().st_size,
        "work": work,
        "dtype": np.dtype(dtype).name,
        "scope": "opaque unfiltered vector field; no global hierarchy, PBR/opacity/shadow or native/device admission",
    }
    (path / "header.json").write_text(json.dumps(header, indent=2) + "\n")
    return header
