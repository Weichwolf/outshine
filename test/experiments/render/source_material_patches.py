"""Source-side material domains and pre-displacement corner fields, not native PBR."""

from dataclasses import dataclass

import numpy as np
from prepared_material_fields import edge_at, facade_catalog
from source_material_domains import domain_axes, shape_material_domains
from source_surface_patches import SurfacePatch


@dataclass
class MaterialPatch:
    geometry: SurfacePatch
    shape: dict
    part: int
    side: int
    gable: bool
    local_origin: np.ndarray
    local_axes: np.ndarray
    surface: dict
    orientation: int


def shape_material_patches(surface, shape, source, part, basis, anchor):
    """Build full world-space patches only for consumers requesting geometry."""
    domains = shape_material_domains(surface, shape)
    frame = np.array([surface[k] for k in ("east", "north", "up")]) @ basis.T
    origin = np.array(surface["origin"]) @ basis.T - anchor
    patches = []
    for d in domains:
        local_origin = np.asarray(d.local_origin, float)
        axes = domain_axes(d, frame)
        if d.side == 1:
            offset = origin + local_origin[2] * frame[2]
        else:
            offset = origin + local_origin[:2] @ frame[:2]
        normal = d.orientation * np.cross(*axes)
        normal /= np.linalg.norm(normal)
        geometry = SurfacePatch(d.domain, offset, axes, normal, source)
        patches.append(
            MaterialPatch(
                geometry,
                shape,
                part,
                d.side,
                d.gable,
                local_origin,
                d.local_axes,
                surface,
                d.orientation,
            )
        )
    return patches


def selected_material_patches(parent, cells, identities, frame):
    selected = set(map(int, identities))
    patches, seen = [], set()
    for _, sources, forms in parent.iter_forms(cells):
        for identity, surface in zip(sources, forms, strict=True):
            identity = int(identity)
            if identity not in selected:
                continue
            if surface is None:
                raise ValueError(f"whole source reference unavailable: {identity}")
            for part, shape in enumerate(surface["shapes"]):
                patches.extend(
                    shape_material_patches(
                        surface, shape, identity, part, frame["basis"], frame["anchor"]
                    )
                )
            seen.add(identity)
    if seen != selected:
        raise ValueError("all selected source domains must remain available")
    return patches


def weld_local(points):
    """BuildingMesh::Snapped: millimetres, ties away from zero."""
    p = np.asarray(points, float)
    if not np.isfinite(p).all() or np.max(abs(p), initial=0) > 1e6:
        raise ValueError("bounded finite source-local coordinates required")
    scaled = p * 1000
    return round_away(scaled) / 1000


def round_away(values):
    # Adding .5 can round the representable value just below a tie upward.
    magnitude = abs(values)
    whole = np.floor(magnitude)
    return np.copysign(whole + (magnitude - whole >= 0.5), values)


def corner_fields(patch, parameters, *, catalog=None):
    """Evaluate UV before weld and masonry metres after weld, before deformation.

    UV uses the known side parameter, avoiding category loss from back-projecting
    quantized world XYZ. Geometry itself remains the unsnapped source control.
    Native position snapping and source-support differences remain separate.
    """
    q = np.asarray(parameters, float)
    if q.ndim != 2 or q.shape[1] != 2 or not np.isfinite(q).all():
        raise ValueError("finite two-dimensional source parameters required")
    p = patch.local_origin + q @ patch.local_axes
    s = patch.shape
    uv = np.zeros((len(q), 2), np.float32)
    metres = np.zeros_like(uv)
    if patch.side in (0, 1):
        kind = 8 if patch.side == 0 else (2 if s["roof_kind"] == 0 else 1)
        code = -(1 + kind + 16 * s["ident"])
        uv[:, 0], uv[:, 1] = code, p[:, 2].astype(np.float32)
        return uv, metres, code
    c = facade_catalog() if catalog is None else catalog
    a, b, entrance, party = edge_at(s, patch.side)
    length = float(np.hypot(*(b - a)))
    parameter_length = float(np.linalg.norm(b - a))
    if length <= 0 or (q[:, 0] < 0).any() or (q[:, 0] > parameter_length).any():
        raise ValueError("source wall parameters outside their actual side")
    bays = (
        0
        if length < 1.9
        else np.clip(round_away(np.asarray(length / s["bay_m"])), 1, 255)
    )
    # wall_domains uses norm for its along parameter; native bay counts use hypot.
    # Normalize by the producer's length, keeping exact endpoint fractions 0/1.
    bay = (
        np.zeros(len(q))
        if patch.gable or party
        else bays * (q[:, 0] / parameter_length)
    )
    standing = 2 if entrance and not patch.gable else 0
    code = s["opening_style"] + len(c) * (standing + 3 * (s["wall_variant"] % 8))
    uv[:, 0] = np.float32(256) * np.float32(code) + bay.astype(np.float32)
    uv[:, 1] = np.float32(1) + (
        (p[:, 2] - s["seat_m"] - s["foot_m"]) / s["floor_m"]
    ).astype(np.float32)
    if s["wall_variant"] == 6 and s["opening_style"] != c["Glazing"]:
        snapped = weld_local(p)
        # Explicit component order matches BuildingWallMaterial::At.
        along = (b - a) * (1 / length)
        metres[:, 0] = (
            (snapped[:, 0] - a[0]) * along[0] + (snapped[:, 1] - a[1]) * along[1]
        ).astype(np.float32)
        metres[:, 1] = (snapped[:, 2] - (s["seat_m"] + s["foot_m"])).astype(np.float32)
    return uv, metres, int(code)


def permute_corner_fields(fields, swap):
    """Carry exact field bits with a geometry winding change, including degenerates."""
    fields, swap = np.asarray(fields), np.asarray(swap)
    if (
        fields.ndim != 3
        or fields.shape[1] != 3
        or swap.shape != fields.shape[:1]
        or swap.dtype != bool
    ):
        raise ValueError("triangle corner fields and matching Boolean swaps required")
    result = fields.copy()
    result[swap] = result[swap][:, [0, 2, 1]]
    return result
