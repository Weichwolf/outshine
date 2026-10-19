"""Source-owned native fields and an unlit layered-depth capture proposal.

Stored samples retain source-triangle/barycentric bindings. Overflow and the
unsampled domain remain explicit; this module grants no occluder coverage.
"""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
import trimesh


def load_source(root):
    receipt = json.loads((root / "mesh/receipt.json").read_text())
    metadata = json.loads((root / "input.json").read_text())
    arrays = {}
    for name, spec in receipt["captures"][0].items():
        data = (root / "mesh/0" / f"{name}.bin").read_bytes()
        if (
            len(data) != spec["bytes"]
            or hashlib.sha256(data).hexdigest() != spec["sha256"]
        ):
            raise ValueError("source stream changed")
        arrays[name] = np.frombuffer(data, dtype="<u4")
    walls = arrays["walls"].reshape(-1, 6)
    roofs = arrays["roofs"].reshape(-1, 6)
    words = np.concatenate((walls, roofs))
    indices = np.concatenate(
        (
            arrays["wall_indices"].reshape(-1, 3),
            arrays["roof_indices"].reshape(-1, 3) + len(walls),
        )
    ).astype(np.uint32)
    if indices.max() >= len(words) or not np.isfinite(words[:, :5].view("<f4")).all():
        raise ValueError("invalid source vertices")
    colour = np.ones((len(words), 4), dtype=np.float32)
    if len(arrays["colours"]):
        colour[: len(walls)] = arrays["colours"].view("<f4").reshape(-1, 4)
    metres = None
    if len(arrays["metres"]):
        metres = np.zeros((len(words), 2), np.float32)
        metres[: len(walls)] = arrays["metres"].view("<f4").reshape(-1, 2)
    uv = words[:, 3:5].view("<f4")
    code = np.where(uv[:, 0] < 0, uv[:, 0], np.floor(uv[:, 0] / 256))
    if (
        (abs(code) > 32767).any()
        or not (code[indices] == code[indices[:, :1]]).all()
        or not (words[indices, 5] == words[indices[:, :1], 5]).all()
        or not (colour[indices] == colour[indices[:, :1]]).all()
    ):
        raise ValueError(
            "this first capture requires flat codes/normals/colour per triangle"
        )
    palette, palette_index = np.unique(
        colour[indices[:, 0]], axis=0, return_inverse=True
    )
    if len(palette) > 65535:
        raise ValueError("bounded exact colour palette required")
    smooth = np.column_stack((np.mod(uv[:, 0], 256), uv[:, 1])).astype(np.float32)
    return {
        "positions": words[:, :3].view("<f4"),
        "indices": indices,
        "normal": words[indices[:, 0], 5],
        "code": code[indices[:, 0]].astype(np.int16),
        "palette": palette,
        "colour_index": palette_index.astype(np.uint16),
        "uv": smooth,
        "metres": metres,
        "domain": np.r_[
            np.zeros(len(arrays["wall_indices"]) // 3, np.uint8),
            np.ones(len(arrays["roof_indices"]) // 3, np.uint8),
        ],
        "metadata": metadata,
        "native_bytes": sum(s["bytes"] for s in receipt["captures"][0].values()),
    }


def capture_view(source, direction, up_hint, *, resolution, layers):
    if (
        type(resolution) is not int
        or not 8 <= resolution <= 512
        or type(layers) is not int
        or not 1 <= layers <= 16
    ):
        raise ValueError("bounded capture dimensions required")
    xyz = source["positions"].astype(float)
    triangles = xyz[source["indices"]]
    direction = np.asarray(direction, float)
    direction /= np.linalg.norm(direction)
    right = np.cross(direction, up_hint)
    right /= np.linalg.norm(right)
    up = np.cross(right, direction)
    basis = np.stack((right, up, direction))
    centre = (xyz.min(0) + xyz.max(0)) / 2
    local = (xyz - centre) @ basis.T
    low, high = local.min(0), local.max(0)
    spacing = (high[:2] - low[:2]) / resolution
    yy, xx = np.indices((resolution, resolution))
    xy = low[:2] + (np.stack((xx, yy), axis=-1) + 0.5) * spacing
    origins = (
        centre
        + xy[:, :, 0, None] * right
        + xy[:, :, 1, None] * up
        + (low[2] - 1) * direction
    )
    origins = origins.reshape(-1, 3)
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    front = np.flatnonzero(normals @ direction < -1e-12)
    mesh = trimesh.Trimesh(xyz, source["indices"][front], process=False)
    if len(front):
        locations, rays, faces = mesh.ray.intersects_location(
            origins, np.broadcast_to(direction, origins.shape), multiple_hits=True
        )
    else:
        locations = np.empty((0, 3))
        rays = np.empty(0, dtype=np.int64)
        faces = np.empty(0, dtype=np.int64)
    faces = front[faces]
    distance = np.einsum("ij,j->i", locations - origins[rays], direction)
    order = np.lexsort((faces, distance, rays))
    locations, rays, faces = locations[order], rays[order], faces[order]
    counts = np.bincount(rays, minlength=len(origins))
    starts = np.r_[0, np.cumsum(counts)[:-1]]
    rank = np.arange(len(rays)) - starts[rays]
    keep = rank < layers
    locations, rays, faces, rank = locations[keep], rays[keep], faces[keep], rank[keep]
    bary = trimesh.triangles.points_to_barycentric(triangles[faces], locations)
    denominator = 1 << 30
    a = np.clip(np.floor(bary[:, 1] * denominator).astype(np.int64), 0, denominator)
    b = np.clip(np.floor(bary[:, 2] * denominator).astype(np.int64), 0, denominator - a)
    weights = np.column_stack((denominator - a - b, a, b)).astype(np.uint32)
    held = np.einsum(
        "ni,nij->nj", weights.astype(float) / denominator, triangles[faces]
    )
    depth = np.einsum("ij,j->i", held - origins[rays], direction)
    transverse = held - origins[rays] - depth[:, None] * direction
    slots = rank * len(origins) + rays
    size = layers * len(origins)
    result = {
        "depth": np.full(size, np.nan, np.float32),
        "normal": np.zeros(size, np.uint32),
        "uv": np.zeros((size, 2), np.float32),
        "code": np.zeros(size, np.int16),
        "colour": np.zeros(size, np.uint16),
        "domain": np.zeros(size, np.uint8),
    }
    result["depth"][slots] = depth
    result["normal"][slots] = source["normal"][faces]
    result["code"][slots] = source["code"][faces]
    result["colour"][slots] = source["colour_index"][faces]
    result["domain"][slots] = source["domain"][faces]
    result["uv"][slots] = np.einsum(
        "ni,nij->nj",
        weights.astype(float) / denominator,
        source["uv"][source["indices"][faces]],
    )
    if source["metres"] is not None:
        result["metres"] = np.zeros((size, 2), np.float32)
        result["metres"][slots] = np.einsum(
            "ni,nij->nj",
            weights.astype(float) / denominator,
            source["metres"][source["indices"][faces]],
        )
    result.update(
        binding_slots=slots.astype(np.uint32),
        binding_faces=faces.astype(np.uint32),
        binding_weights=weights,
        basis=basis,
        centre=centre,
        low=low,
        high=high,
        spacing=spacing,
        counts=counts.astype(np.uint32),
    )
    info = {
        "resolution": resolution,
        "layers": layers,
        "rays": len(origins),
        "stored_samples": len(slots),
        "ray_hits": int(counts.sum()),
        "overflow_rays": int((counts > layers).sum()),
        "dropped_hits": int(np.maximum(counts.astype(np.int64) - layers, 0).sum()),
        "max_binding_transverse_m": float(
            np.linalg.norm(transverse, axis=1).max(initial=0)
        ),
        "max_depth_float32_error_m": float(
            abs(depth.astype(np.float32).astype(float) - depth).max(initial=0)
        ),
        "dense_runtime_field_bytes": sum(
            v.nbytes
            for k, v in result.items()
            if k in ("depth", "normal", "uv", "code", "colour", "domain", "metres")
        ),
        "sample_scope": "Original-triangle-bound unlit fields at sampled rays only. No coverage claim between samples or at a new eye.",
    }
    return result, info


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--resolution", type=int, default=64)
    p.add_argument("--layers", type=int, default=4)
    p.add_argument("--hull", type=Path)
    a = p.parse_args()
    out = a.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    source = load_source(a.input)
    anchor = np.array(json.loads((a.input / "native.json").read_text())["anchor_ecef"])
    up = anchor / np.linalg.norm(anchor)
    east = np.cross([0, 0, 1], up)
    east /= np.linalg.norm(east)
    north = np.cross(up, east)
    out.mkdir(parents=True, exist_ok=False)
    records = []
    start = time.perf_counter()
    for i, direction in enumerate((east, -east, north, -north, up, -up)):
        data, info = capture_view(
            source,
            direction,
            north if abs(np.dot(direction, up)) > 0.9 else up,
            resolution=a.resolution,
            layers=a.layers,
        )
        np.savez(out / f"view-{i}.npz", **data)
        records.append(info)
        print(json.dumps({"view": i, **info}), flush=True)
    hull = None
    if a.hull:
        other = load_source(a.hull)
        for name in ("subset.index", "subset.forms"):
            if (a.input / name).read_bytes() != (a.hull / name).read_bytes():
                raise ValueError("hull changes source bytes")
        hull = {
            "native_bytes": other["native_bytes"],
            "triangles": len(other["indices"]),
            "source": other["metadata"],
        }
    valid = sum(r["stored_samples"] for r in records)
    dense = sum(r["dense_runtime_field_bytes"] for r in records)
    per_sample = 21 + (8 if source["metres"] is not None else 0)
    report = {
        "source": source["metadata"],
        "native_fine_bytes": source["native_bytes"],
        "native_fine_triangles": len(source["indices"]),
        "palette": source["palette"].tolist(),
        "views": records,
        "dense_runtime_field_bytes": dense,
        "sparse_field_and_offset_estimate_bytes": valid * per_sample
        + 6 * a.resolution * a.resolution * 4
        + source["palette"].nbytes,
        "sparse_estimate_scope": "Fields plus one uint32 offset per ray, excluding alignment, overflow/fallback metadata and bind/state overhead; not measured GPU memory.",
        "hull": hull,
        "seconds": time.perf_counter() - start,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "code_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "unresolved": [
            "coverage between samples",
            "camera-domain validity",
            "view switching/disocclusion",
            "native material evaluation",
            "near/far transitions",
            "native device cost",
        ],
        "scope": "Source-owned proposals from unchanged native Fine. Depth/normal/material state is unlit. Sample bindings are not a global coverage/appearance certificate. Original source stays available.",
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
