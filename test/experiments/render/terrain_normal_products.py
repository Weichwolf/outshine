"""Prepared Float32 terrain differences/normals; source/basis ownership stays explicit."""

import hashlib
from dataclasses import dataclass

import numpy as np


def _owned(value, dtype):
    result = np.array(value, dtype=dtype, copy=True)
    result.setflags(write=False)
    return result


@dataclass(frozen=True, slots=True)
class HeightSource:
    heights: np.ndarray
    step_m: np.ndarray
    digest: str
    binding: tuple


@dataclass(frozen=True, slots=True)
class NormalProduct:
    kind: str
    fields: tuple
    source_digest: str
    step_words: bytes


@dataclass(frozen=True, slots=True)
class BoundNormals:
    source: HeightSource
    product: object
    basis: np.ndarray


def source(heights, step_m, *, binding=()):
    if (
        not isinstance(heights, np.ndarray)
        or heights.dtype != np.float32
        or heights.shape != (35, 35)
        or not np.isfinite(heights).all()
    ):
        raise ValueError(
            "complete finite35x35Float32 height page including halo required"
        )
    step = np.asarray(step_m, dtype=np.float32)
    if step.shape != (2,) or not np.isfinite(step).all() or np.any(step <= 0):
        raise ValueError("positive finite two-component Float32 metric step required")
    height = _owned(heights, np.float32)
    step = _owned(np.maximum(step, np.float32(0.001)), np.float32)
    digest = hashlib.sha256(height.astype("<f4", copy=False).tobytes()).hexdigest()
    return HeightSource(height, step, digest, tuple(binding))


def _normalize(v):
    length = np.sqrt((v[:, 0] * v[:, 0] + v[:, 1] * v[:, 1]) + v[:, 2] * v[:, 2])
    return v / length[:, None]


def _local(de, dn, step):
    denominator = np.float32(2) * step
    return _normalize(
        np.column_stack(
            (
                -de / denominator[0],
                -dn / denominator[1],
                np.ones(len(de), dtype=np.float32),
            )
        )
    )


def build(src, kind):
    if kind not in ("differences", "rgba_normals"):
        raise ValueError("known prepared terrain-normal field required")
    h = src.heights
    de = h[1:-1, 2:] - h[1:-1, :-2]
    dn = h[:-2, 1:-1] - h[2:, 1:-1]
    if kind == "differences":
        fields, steps = (_owned(de, np.float32), _owned(dn, np.float32)), b""
    else:
        normal = _local(de.ravel(), dn.ravel(), src.step_m).reshape(33, 33, 3)
        rgba = np.zeros((33, 33, 4), dtype=np.float32)
        rgba[..., :3] = normal
        fields, steps = (_owned(rgba, np.float32),), src.step_m.tobytes()
    if not all(np.isfinite(f).all() for f in fields):
        raise ValueError("finite prepared terrain-normal values required")
    return NormalProduct(kind, fields, src.digest, steps)


def bind(src, product=None, *, basis=None):
    frame = (
        np.eye(3, dtype=np.float32)
        if basis is None
        else np.asarray(basis, dtype=np.float32)
    )
    if (
        frame.shape != (3, 3)
        or not np.isfinite(frame).all()
        or np.linalg.det(frame) == 0
    ):
        raise ValueError("finite nonsingular source-normal basis required")
    if product is not None and (
        product.source_digest != src.digest
        or (product.step_words and product.step_words != src.step_m.tobytes())
    ):
        raise ValueError("stale source/metric binding for prepared terrain normals")
    return BoundNormals(src, product, _owned(frame, np.float32))


def _nodes(nodes, max_queries):
    if type(max_queries) is not int or max_queries < 0 or len(nodes) > max_queries:
        raise ValueError("bounded normal query count required before conversion")
    nodes = np.asarray(nodes)
    if (
        nodes.ndim != 2
        or nodes.shape[1:] != (2,)
        or nodes.dtype.kind not in "iu"
        or np.any(nodes < 0)
        or np.any(nodes > 32)
    ):
        raise ValueError("qualified integer row/column nodes in0..32 required")
    return nodes[:, 0], nodes[:, 1]


def sample(bound, nodes, *, max_queries=262144, max_words=1048576, cancelled=None):
    if cancelled is not None and cancelled():
        raise ValueError("terrain-normal query cancelled")
    row, col = _nodes(nodes, max_queries)
    kind = "height_stencil" if bound.product is None else bound.product.kind
    per_query_words = 2 if kind == "differences" else 4
    if (
        type(max_words) is not int
        or max_words < 0
        or len(row) * per_query_words > max_words
    ):
        raise ValueError("terrain-normal source-word budget exhausted")
    if kind == "height_stencil":
        h = bound.source.heights
        de = h[row + 1, col + 2] - h[row + 1, col]
        dn = h[row, col + 1] - h[row + 2, col + 1]
        local = _local(de, dn, bound.source.step_m)
    elif kind == "differences":
        de, dn = (f[row, col] for f in bound.product.fields)
        local = _local(de, dn, bound.source.step_m)
    else:
        rgba = bound.product.fields[0][row, col]
        local = rgba[:, :3]
    b = bound.basis
    world = np.column_stack(
        tuple(
            (local[:, 0] * b[i, 0] + local[:, 1] * b[i, 1]) + local[:, 2] * b[i, 2]
            for i in range(3)
        )
    )
    normal = _normalize(world)
    if cancelled is not None and cancelled():
        raise ValueError("terrain-normal query cancelled")
    if not np.isfinite(normal).all():
        raise ValueError("unresolved nonfinite source normal; no partial result")
    return {
        "normals": normal,
        "binding": bound.source.binding,
        "work": {
            "normal_queries": len(row),
            "nominal_texture_fetches": len(row)
            * (2 if kind == "differences" else 1 if kind == "rgba_normals" else 4),
            "nominal_texture_float_words": len(row) * per_query_words,
            "selected_input_gather_bytes": len(row) * per_query_words * 4,
            "local_normalizations": 0 if kind == "rgba_normals" else len(row),
            "world_normalizations": len(row),
            "result_bytes": normal.nbytes,
        },
    }


def sample_fine(bound, uv, *, max_fragments=65536, cancelled=None):
    if type(max_fragments) is not int or max_fragments < 0 or len(uv) > max_fragments:
        raise ValueError("bounded fine-normal fragment count required")
    uv = np.asarray(uv, dtype=np.float32)
    if uv.ndim != 2 or uv.shape[1:] != (2,) or not np.isfinite(uv).all():
        raise ValueError("finite Nx2 source UV required")
    at = np.clip(uv * np.float32(32), np.float32(0), np.float32(32))
    cell = np.minimum(np.floor(at).astype(np.int32), 31)
    f = at - cell.astype(np.float32)
    first = cell[:, ::-1]
    diagonal = first + 1
    middle = first + np.where((f[:, 0] >= f[:, 1])[:, None], [0, 1], [1, 0])
    result = sample(
        bound,
        np.stack((first, middle, diagonal), axis=1).reshape(-1, 2),
        cancelled=cancelled,
    )
    values = result["normals"].reshape(-1, 3, 3)
    upper, lower = np.maximum(f[:, 0], f[:, 1]), np.minimum(f[:, 0], f[:, 1])
    result["normals"] = (
        (np.float32(1) - upper)[:, None] * values[:, 0]
        + (upper - lower)[:, None] * values[:, 1]
    ) + lower[:, None] * values[:, 2]
    result["work"]["fine_fragments"] = len(uv)
    result["work"]["result_bytes"] = result["normals"].nbytes
    return result
