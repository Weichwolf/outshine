"""Existing BuildingSurface coordinate rules, not a reconstructed PBR material."""

import re
from pathlib import Path

import numpy as np


def facade_catalog(path=None):
    path = (
        path
        or Path(__file__).resolve().parents[3] / "src/content/shade/FacadeProfiles.inc"
    )
    rows = re.findall(
        r"^FACADE_PROFILE\((\w+),\s*(\d+),", path.read_text(), re.MULTILINE
    )
    if not rows or [int(i) for _, i in rows] != list(range(len(rows))):
        raise ValueError("contiguous owned facade catalog required")
    return {name: int(value) for name, value in rows}


def edge_at(shape, face):
    at = face - 2
    if at < 0:
        raise ValueError("wall face required")
    for i, ring in enumerate([shape["ring"], *shape["holes"]]):
        if at < len(ring):
            return (
                np.array(ring[at]),
                np.array(ring[(at + 1) % len(ring)]),
                i == 0 and at == shape["front_edge"],
                i == 0 and shape["party_wall_edges"][at] != 0,
            )
        at -= len(ring)
    raise ValueError("unknown prepared wall face")


def at_local(shape, face, points, *, gable=None, catalog=None):
    """Current TextureAt/MaterialMetresAt formulas and Float32 cast sites.

    Point coordinates are original source-local ENU metres. This is the analytic
    BuildingSurface rule, which may differ from a cached mesher's field recipe.
    No missing style/floor/bay/identifier is inferred from observed mesh UVs.
    """
    p = np.asarray(points, float)
    if p.ndim != 2 or p.shape[1] != 3 or not np.isfinite(p).all():
        raise ValueError("finite local material points required")
    c = facade_catalog() if catalog is None else catalog
    uv = np.zeros((len(p), 2), np.float32)
    metres = np.zeros_like(uv)
    if face < 2:
        if face < 0:
            raise ValueError("nonnegative prepared face required")
        kind = 8 if face == 0 else (2 if shape["roof_kind"] == 0 else 1)
        uv[:, 0] = np.float32(-(1 + kind + 16 * shape["ident"]))
        uv[:, 1] = p[:, 2].astype(np.float32)
        return uv, metres
    a, b, entrance, party = edge_at(shape, face)
    e = b - a
    length = np.hypot(*e)
    fraction = (
        ((p[:, :2] - a) @ e) / (length * length) if length > 0 else np.zeros(len(p))
    )
    if gable is None:
        raise ValueError("wall evaluation requires the hit's explicit gable context")
    gable = np.broadcast_to(np.asarray(gable, bool), (len(p),))
    bays = (
        0.0
        if length < 1.9
        else np.clip(np.floor(length / shape["bay_m"] + 0.5), 1, 255)
    )
    bay = np.where(gable | party, 0.0, bays * fraction).astype(np.float32)
    standing = np.where(entrance & ~gable, 2, 0)
    code = shape["opening_style"] + len(c) * (
        standing + 3 * (shape["wall_variant"] % 8)
    )
    uv[:, 0] = np.float32(256) * code.astype(np.float32) + bay
    uv[:, 1] = np.float32(1) + (
        (p[:, 2] - shape["seat_m"] - shape["foot_m"]) / shape["floor_m"]
    ).astype(np.float32)
    if shape["wall_variant"] == 6 and shape["opening_style"] != c["Glazing"]:
        along = e / length if length > 0 else e
        metres[:, 0] = ((p[:, :2] - a) @ along).astype(np.float32)
        metres[:, 1] = (p[:, 2] - (shape["seat_m"] + shape["foot_m"])).astype(
            np.float32
        )
    return uv, metres


def at_world(surface, part, face, points, *, gable=None, catalog=None):
    basis = np.array([surface[k] for k in ("east", "north", "up")])
    local = (np.asarray(points, float) - surface["origin"]) @ basis.T
    return at_local(surface["shapes"][part], face, local, gable=gable, catalog=catalog)
