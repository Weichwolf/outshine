"""Bounded once-per-prototype leaf packing, preserving UVs and source organ runs."""

import numpy as np
from biome_models import Assembly
from spatial_crown_replay import check_mesh


def preflight(rows, meshes, max_bytes, position_error):
    ids, variants = rows["leaf_ids"], rows["leaf_variants"]
    origins, frames = rows["leaf_origins"], rows["leaf_frames"]
    count = len(ids)
    if (
        ids.shape != (count,)
        or ids.dtype.kind not in "iu"
        or count > 100000
        or np.any(ids < 0)
        or len(np.unique(ids)) != count
        or variants.shape != (count,)
        or variants.dtype.kind not in "iu"
        or not 1 <= len(meshes) <= 64
        or np.any((variants < 0) | (variants >= len(meshes)))
        or origins.shape != (count, 3)
        or frames.shape != (count, 3, 3)
        or not np.isfinite(origins).all()
        or not np.isfinite(frames).all()
        or not np.allclose(
            frames @ frames.transpose(0, 2, 1), np.eye(3), atol=2e-12, rtol=0
        )
        or not np.allclose(np.linalg.det(frames), 1, atol=2e-12, rtol=0)
        or type(max_bytes) is not int
        or not 1 <= max_bytes <= 512 * 1024**2
        or not np.isfinite(position_error)
        or not 0 < position_error <= 1e-3
    ):
        raise ValueError(
            "bounded source organs, rigid frames and explicit packing budget required"
        )
    counts = np.bincount(variants.astype(int), minlength=len(meshes))
    vertex_count = face_count = 0
    extent = 0.0
    for count, mesh in zip(counts, meshes):
        check_mesh(mesh)
        if (
            mesh["corner_uv"].shape != (mesh["faces"].size, 2)
            or not np.isfinite(mesh["corner_uv"]).all()
        ):
            raise ValueError("complete finite source corner UVs required")
        if mesh["surface"].shape != (len(mesh["faces"]),) or np.any(
            mesh["surface"] != mesh["surface"][0]
        ):
            raise ValueError(
                "one declared material per physical leaf prototype required"
            )
        vertex_count += int(count) * len(mesh["vertices"])
        face_count += int(count) * len(mesh["faces"])
        extent = max(extent, float(np.linalg.norm(mesh["vertices"], axis=1).max()))
    byte_count = (
        vertex_count * 12 + face_count * (12 + 36 + 24 + 1) + len(ids) * (8 + 1 + 4 * 4)
    )
    rounding_bound = (
        float(np.finfo(np.float32).eps)
        / 2
        * max(1.0, float(abs(origins).max(initial=0)) + extent)
    )
    if vertex_count > 2**32 - 1 or face_count > 2**32 - 1 or byte_count > max_bytes:
        raise ValueError(
            "complete packed leaf product exceeds the explicit buffer budget"
        )
    if rounding_bound > position_error:
        raise ValueError(
            "tree-local coordinates exceed the requested Float32 error budget"
        )
    return {
        "vertices": vertex_count,
        "faces": face_count,
        "output_bytes": byte_count,
        "position_rounding_bound_m": rounding_bound,
    }


def pack_leaves(
    rows, meshes, *, packet=1024, max_bytes=256 * 1024**2, position_error=1e-6
):
    if type(packet) is not int or not 1 <= packet <= 4096:
        raise ValueError("bounded packing packet required")
    report = preflight(rows, meshes, max_bytes, position_error)
    assembly = Assembly()
    runs = []
    vbase = fbase = 0
    for variant, mesh in enumerate(meshes):
        selected = np.flatnonzero(rows["leaf_variants"] == variant)
        nv, nf = len(mesh["vertices"]), len(mesh["faces"])
        for first in range(0, len(selected), packet):
            source = selected[first : first + packet]
            count = len(source)
            assembly.add(
                mesh,
                rows["leaf_origins"][source],
                rows["leaf_frames"][source],
                int(mesh["surface"][0]),
            )
            runs.append(
                {
                    "ids": rows["leaf_ids"][source].astype(np.uint64),
                    "variants": np.full(count, variant, np.uint8),
                    "first_vertex": vbase + np.arange(count, dtype=np.uint32) * nv,
                    "vertices": np.full(count, nv, np.uint32),
                    "first_face": fbase + np.arange(count, dtype=np.uint32) * nf,
                    "faces": np.full(count, nf, np.uint32),
                }
            )
            vbase += count * nv
            fbase += count * nf
    if not runs:
        mesh = {
            "vertices": np.empty((0, 3), np.float32),
            "faces": np.empty((0, 3), np.uint32),
            "normals": np.empty((0, 3), np.float32),
            "corner_uv": np.empty((0, 2), np.float32),
            "surface": np.empty(0, np.uint8),
        }
        run = {
            k: np.empty(
                0,
                np.uint64 if k == "ids" else np.uint8 if k == "variants" else np.uint32,
            )
            for k in (
                "ids",
                "variants",
                "first_vertex",
                "vertices",
                "first_face",
                "faces",
            )
        }
        return (
            mesh,
            run,
            report | {"maximum_position_error_m": 0.0, "maximum_normal_error": 0.0},
        )
    raw = assembly.finish()
    del assembly
    raw["corner_uv"] = raw.pop("uv")
    result = {
        k: a.astype(
            np.uint32 if k == "faces" else np.uint8 if k == "surface" else np.float32
        )
        for k, a in raw.items()
    }
    error = float(
        abs(result["vertices"].astype(float) - raw["vertices"]).max(initial=0)
    )
    normal_error = float(
        abs(result["normals"].astype(float) - raw["normals"]).max(initial=0)
    )
    if error > position_error or normal_error > 1e-6:
        raise ValueError(
            "actual geometry quantization exceeded the declared error budget"
        )
    run = {k: np.concatenate([r[k] for r in runs]) for k in runs[0]}
    return (
        result,
        run,
        report
        | {"maximum_position_error_m": error, "maximum_normal_error": normal_error},
    )
