"""Fixed-order rigid/uniform surface assembly with hierarchical vertex identities."""

import numpy as np

STEM = np.iinfo(np.uint32).max


def combine(parts):
    offset, faces = 0, []
    for part in parts:
        faces.append(part["faces"] + offset)
        offset += len(part["vertices"])
    return {
        key: np.concatenate([p[key] for p in parts])
        for key in parts[0]
        if key != "faces"
    } | {"faces": np.concatenate(faces).astype(np.int32, copy=False)}


def source(mesh, identity, surface):
    uv = mesh.get("corner_uv")
    if uv is None:
        if "parameters" in mesh:
            q = mesh["parameters"]
            uv = np.c_[(q[:, 1] + 1) / 2, q[:, 0]][mesh["faces"].ravel()]
        else:
            uv = np.zeros((mesh["faces"].size, 2))
    return {
        "vertices": mesh["vertices"],
        "faces": mesh["faces"].astype(np.int32),
        "normals": mesh["normals"],
        "corner_uv": uv,
        "organ_ids": np.full((len(mesh["vertices"]), 1), identity, np.uint32),
        "surface": np.full(len(mesh["faces"]), surface, np.uint8),
    }


def repeat(mesh, origins, frames, scales, tags):
    origins, frames, scales, tags = map(np.asarray, (origins, frames, scales, tags))
    count = len(origins)
    if (
        origins.shape != (count, 3)
        or frames.shape != (count, 3, 3)
        or scales.shape != (count,)
        or tags.shape != (count,)
        or not 1 <= count <= 256
        or len(mesh["vertices"]) * count > 1_000_000
        or len(mesh["faces"]) * count > 2_000_000
        or not all(np.isfinite(a).all() for a in (origins, frames, scales))
        or np.any(scales <= 0)
        or tags.dtype.kind not in "iu"
        or np.any((tags < 0) | (tags > STEM))
        or np.max(abs(np.einsum("nji,njk->nik", frames, frames) - np.eye(3))) > 2e-12
        or np.max(abs(np.linalg.det(frames) - 1)) > 2e-12
    ):
        raise ValueError(
            "bounded rigid placements and positive uniform scales required"
        )
    v = mesh["vertices"][None] * scales[:, None, None]
    # Explicit three-term order preserves exact subset/full replay independently of batch size.
    vertices = (
        sum(frames[:, None, :, k] * v[:, :, k, None] for k in range(3))
        + origins[:, None]
    )
    n = mesh["normals"]
    normals = sum(frames[:, None, :, k] * n[None, :, k, None] for k in range(3))
    faces = mesh["faces"][None] + np.arange(count, dtype=np.int32)[:, None, None] * len(
        mesh["vertices"]
    )
    return {
        "vertices": vertices.reshape(-1, 3),
        "faces": faces.reshape(-1, 3),
        "normals": normals.reshape(-1, 3),
        "corner_uv": np.tile(mesh["corner_uv"], (count, 1)),
        "organ_ids": np.c_[
            np.repeat(tags, len(mesh["vertices"])),
            np.tile(mesh["organ_ids"], (count, 1)),
        ].astype(np.uint32),
        "surface": np.tile(mesh["surface"], count),
    }
