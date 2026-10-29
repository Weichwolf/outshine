"""Exact original triangle operands; no scene-frame reconstruction or transforms."""

import hashlib
import json

import numpy as np
from building_sample_inputs import digest


def read_original(directory):
    manifest = json.loads((directory / "manifest.json").read_text())
    parent_path = directory.parent / "manifest.json"
    parent = json.loads(parent_path.read_text())
    path = directory / "geometry.npz"
    if (
        manifest["version"] != 1
        or manifest["parent_manifest_sha256"] != digest(parent_path)
        or digest(path) != manifest["npz_sha256"]
        or path.stat().st_size != manifest["npz_bytes"]
        or {k: v["sha256"] for k, v in manifest["files"].items()}
        != parent["original_geometry_files"]
    ):
        raise ValueError("Identical native original geometry required")
    values = {}
    with np.load(path, allow_pickle=False) as saved:
        if set(saved.files) != {v["array"] for v in manifest["files"].values()}:
            raise ValueError("Exact original operand inventory required")
        for name, binding in manifest["files"].items():
            value = saved[binding["array"]]
            if (
                value.dtype != np.dtype(binding["dtype"])
                or list(value.shape) != binding["shape"]
                or value.nbytes != binding["bytes"]
                or not np.isfinite(value).all()
                or hashlib.sha256(value.tobytes()).hexdigest() != binding["sha256"]
            ):
                raise ValueError("Original geometry operand changed: " + name)
            values[name] = value
    parts = json.loads(values["input.json"].tobytes())["parts"]
    vertices, faces, offset = [], [], 0
    for index, part in enumerate(parts):
        xyz = values[f"{index}/positions.bin"].reshape(part["vertices"], 3)
        ids = values[f"{index}/indices.bin"].reshape(part["triangles"], 3)
        for name, columns in (("normals", 3), ("uv", 2), ("uv1", 2), ("colours", 4)):
            if values[f"{index}/{name}.bin"].size != part["vertices"] * columns:
                raise ValueError("Original vertex attribute count changed")
        if ids.size and ids.max() >= len(xyz):
            raise ValueError("Original triangle index is outside its part")
        vertices.append(xyz)
        faces.append(ids.astype(np.int64) + offset)
        offset += len(xyz)
    source = {"positions": np.concatenate(vertices), "indices": np.concatenate(faces)}
    if (
        len(source["positions"]) != manifest["vertices"]
        or len(source["indices"]) != manifest["triangles"]
        or not np.array_equal(source["positions"].min(0), parent["bounds"]["least"])
        or not np.array_equal(source["positions"].max(0), parent["bounds"]["most"])
    ):
        raise ValueError("Exact original topology/bounds changed")
    return source, {
        "source_count": manifest["source_count"],
        "triangles": manifest["triangles"],
        "vertices": manifest["vertices"],
        "verified_files": len(values),
        "decoded_operand_bytes": sum(v.nbytes for v in values.values()),
        "joined_position_bytes": source["positions"].nbytes,
        "joined_int64_index_bytes": source["indices"].nbytes,
        "scope": "All19 file hashes match actual native readbacks. Float32 positions unmodified; part indices offset in int64. Group source count preserved; no invented per-face SourceIDs or transforms.",
    }


def centre_control(row, native_depth, manifest):
    depth = np.array(
        [np.inf if v is None else v for v in row["centre_forward_depth_m"]]
    ).reshape(row["centre_shape"])
    x, y = np.array(row["centre_origin"]) - manifest["crop"][:2]
    h, w = depth.shape
    if x < 0 or y < 0 or x + w > native_depth.shape[1] or y + h > native_depth.shape[0]:
        raise ValueError("Original centre rays lie outside the native crop")
    native = native_depth[y : y + h, x : x + w, 0]
    cpu_mask, native_mask = np.isfinite(depth), native > 0
    both = cpu_mask & native_mask
    residual = abs(float(np.float32(0.1)) / native[both].astype(float) - depth[both])
    return {
        "cpu_hits": int(cpu_mask.sum()),
        "native_hits": int(native_mask.sum()),
        "coverage_mismatches": int(np.count_nonzero(cpu_mask != native_mask)),
        "native_hits_outside_cpu_bounds": int(
            np.count_nonzero(native_depth) - native_mask.sum()
        ),
        "common_hits": int(both.sum()),
        "maximum_forward_depth_residual_m": float(residual.max())
        if len(residual)
        else None,
        "median_forward_depth_residual_m": float(np.median(residual))
        if len(residual)
        else None,
        "scope": "Independent CPU centre rays versus actual native base-frame reverse-Z depth; Float32 near0.1m. Records every mask mismatch, not a shading/SS64 oracle or a backend error bound.",
    }
