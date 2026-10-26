"""Camera/light-independent wall/roof reflectance measures before fine meshes."""

import hashlib
import json

import numpy as np
import shapely
from building_mass_materials import material
from prepared_material_fields import edge_at
from source_material_domains import domain_axes, shape_material_domains


def policy_digest(at):
    raw = json.dumps(
        at["sources_sha256"], sort_keys=True, separators=(",", ":")
    ).encode()
    return hashlib.sha256(raw).digest()


def patch_normal_areas(patches):
    """Per-shape batches; retain the scalar norm arithmetic for each row."""
    if not patches:
        return np.empty((0, 3)), np.empty(0)
    local = np.stack([p.local_axes for p in patches])
    axes = np.stack([p.geometry.axes for p in patches])
    return normal_areas(local, axes, [p.orientation for p in patches])


def normal_areas(local, axes, orientations):
    normals = np.array(orientations)[:, None] * np.cross(local[:, 0], local[:, 1])
    for normal in normals:
        normal /= np.linalg.norm(normal)
    areas = np.array([np.linalg.norm(n) for n in np.cross(axes[:, 0], axes[:, 1])])
    return normals, areas


def patch_measure(patch, at, *, normal_area=None, material_lookup=None):
    if normal_area is None:
        local_normal = patch.orientation * np.cross(*patch.local_axes)
        local_normal /= np.linalg.norm(local_normal)
        jacobian = np.linalg.norm(np.cross(*patch.geometry.axes))
    else:
        local_normal, jacobian = normal_area
    return domain_measure(
        patch.geometry.domain,
        patch.shape,
        patch.surface,
        patch.side,
        patch.gable,
        at,
        (local_normal, jacobian),
        material_lookup=material_lookup,
    )


def material_measure(at, code, stream, paint, **kwargs):
    colour, weights = material(at, code, stream, paint, **kwargs)
    return np.concatenate((colour, weights, [1]))


def clipped_wall_parts(domain, base):
    x0, z0, x1, z1 = domain.bounds
    for above, low, high in ((False, z0, min(z1, base)), (True, max(z0, base), z1)):
        if high > low:
            yield (
                above,
                shapely.intersection(domain, shapely.box(x0, low, x1, high)).area,
            )


def clipped_wall_areas(domains, base):
    result = np.zeros((len(domains), 2))
    if not domains:
        return result
    bounds = shapely.bounds(domains)
    lower = np.column_stack((bounds[:, 1], np.maximum(bounds[:, 1], base)))
    upper = np.column_stack((np.minimum(bounds[:, 3], base), bounds[:, 3]))
    rows, cols = np.nonzero(upper > lower)
    boxes = shapely.box(
        bounds[rows, 0], lower[rows, cols], bounds[rows, 2], upper[rows, cols]
    )
    result[rows, cols] = shapely.area(
        shapely.intersection(np.asarray(domains, object)[rows], boxes)
    )
    return result


def domain_measure(
    domain,
    shape,
    surface,
    side,
    gable,
    at,
    normal_area,
    *,
    material_lookup=None,
    wall_areas=None,
):
    local_normal, jacobian = normal_area
    stream = int(local_normal[2] > 0.5)
    lookup = material_measure if material_lookup is None else material_lookup
    colour = surface["wall_colour"]
    paint = np.ones(3) if colour is None else np.asarray(colour) / at["base"][0]
    result = np.zeros(4 + len(at["palette"]))
    if side == 1:
        trim = 2 if shape["roof_kind"] == 0 else 1
        code = -(1 + trim + 16 * shape["ident"])
        if int(np.float32(code)) != code:
            raise ValueError(
                "roof category cannot be represented by the source UV word"
            )
        slope = np.sqrt(max(0, 1 - local_normal[2] ** 2))
        response = lookup(at, code, stream, paint, slope=slope)
        area = domain.area * jacobian
        result[:] = response * area
        return result, 1
    a, b, entrance, party = edge_at(shape, side)
    opening = not (gable or party) and np.linalg.norm(b - a) >= 1.9
    code = shape["opening_style"] + len(at["fractions"]) * (
        (2 if entrance and not gable else 0) + 3 * (shape["wall_variant"] % 8)
    )
    base = shape["seat_m"] + shape["foot_m"]
    calls = 0
    parts = (
        clipped_wall_parts(domain, base)
        if wall_areas is None
        else enumerate(wall_areas)
    )
    for above, local_area in parts:
        above = bool(above)
        area = local_area * jacobian
        if area <= 0:
            continue
        response = lookup(
            at,
            code,
            stream,
            paint,
            openings=above and opening,
            storeys_per_m=(1 / shape["floor_m"] if above else 0),
        )
        result += response * area
        calls += 1
    return result, calls


def source_measure(identity, surface, frame, at):
    if surface is None:
        raise ValueError("original source material forms unavailable")
    if not surface["shapes"]:
        raise ValueError("positive finite source wall/roof measures required")
    result = np.zeros((2, 4 + len(at["palette"])))
    states = {}

    def lookup(policy, code, stream, paint, **kwargs):
        key = (
            code,
            stream,
            paint.dtype.str,
            paint.shape,
            paint.tobytes(),
            tuple(sorted(kwargs.items())),
        )
        if key not in states:
            states[key] = material_measure(policy, code, stream, paint, **kwargs)
        return states[key]

    patches = 0
    transform = (
        np.array([surface[k] for k in ("east", "north", "up")]) @ frame["basis"].T
    )
    for shape in surface["shapes"]:
        generated = shape_material_domains(surface, shape)
        if not generated:
            continue
        local = np.stack([p.local_axes for p in generated])
        axes = np.stack([domain_axes(p, transform) for p in generated])
        normals, areas = normal_areas(local, axes, [p.orientation for p in generated])
        wall_areas = iter(
            clipped_wall_areas(
                [p.domain for p in generated if p.side != 1],
                shape["seat_m"] + shape["foot_m"],
            )
        )
        for patch, normal, area in zip(generated, normals, areas, strict=True):
            value, _ = domain_measure(
                patch.domain,
                shape,
                surface,
                patch.side,
                patch.gable,
                at,
                (normal, area),
                material_lookup=lookup,
                wall_areas=None if patch.side == 1 else next(wall_areas),
            )
            result[int(patch.side == 1)] += value
            patches += 1
    if np.any(result[:, -1] <= 0) or not np.isfinite(result).all():
        raise ValueError("positive finite source wall/roof measures required")
    return result, patches, len(states)


def group_materials(measures, geometry, at):
    count = len(geometry["group_offsets"]) - 1
    colour = np.zeros((count, 2, 3), np.float32)
    weights = np.zeros((count, 2, len(at["palette"])), np.float32)
    area = np.zeros((count, 2), np.float32)
    supported = np.zeros(count, bool)
    for group in range(count):
        low, high = geometry["group_offsets"][group : group + 2]
        sources = geometry["group_sources"][low:high]
        if not all(int(i) in measures for i in sources):
            continue
        total = np.sum([measures[int(i)] for i in sources], axis=0)
        colour[group] = total[:, :3] / total[:, -1, None]
        weights[group] = total[:, 3:-1] / total[:, -1, None]
        area[group], supported[group] = total[:, -1], True
    arrays = {
        "albedo": colour,
        "roughness_weights": weights,
        "surface_area_m2": area,
        "supported": supported,
        "roughness_palette": at["palette"],
        "policy_sha256": np.frombuffer(policy_digest(at), np.uint8).copy(),
    }
    return arrays


def build_appearance(parent, cells, geometry, at):
    selected = set(map(int, geometry["selected_sources"]))
    grouped = set(map(int, geometry["group_sources"]))
    measures, needs, patch_count, calls = {}, [], 0, 0
    seen = set()
    for _, identities, forms in parent.iter_forms(cells):
        for identity, surface in zip(identities, forms, strict=True):
            identity = int(identity)
            seen.add(identity)
            if identity not in grouped:
                continue
            try:
                measure, patches, evaluations = source_measure(
                    identity, surface, geometry, at
                )
            except (
                ValueError,
                np.linalg.LinAlgError,
                shapely.errors.GEOSException,
            ) as error:
                needs.append({"source": identity, "reason": str(error)})
                continue
            measures[identity] = measure
            patch_count += patches
            calls += evaluations
    if seen != selected:
        raise ValueError(
            "geometry and material producer must cover the same original sources"
        )
    count = len(geometry["group_offsets"]) - 1
    arrays = group_materials(measures, geometry, at)
    return arrays, {
        "source_needs": needs,
        "sources_with_measures": len(measures),
        "retained_sources_without_new_material_work": len(selected - grouped),
        "source_surface_patches": patch_count,
        "material_evaluations": calls,
        "retained_source_measure_array_bytes": sum(v.nbytes for v in measures.values()),
        "known_patch_accumulator_array_bytes": patch_count
        * (4 + len(at["palette"]))
        * 8,
        "form_reads": parent.form_reads,
        "form_bytes": parent.form_bytes,
        "supported_groups": int(arrays["supported"].sum()),
        "all_groups": count,
        "array_bytes": {k: v.nbytes for k, v in arrays.items()},
        "all_array_bytes": sum(v.nbytes for v in arrays.values()),
        "policy_sources_sha256": at["sources_sha256"],
        "scope": "Source-area wall/roof steady-repeat opaque reflectance approximation, not projected visible-area statistics or a native filtered-shader match. No normal averaging or baked lighting; source geometry stays in the frozen mass product. Finite boundaries, visibility/material correlation, full source shading normals, overhang/recessed/base geometry, sheen/transmission/emission and native admission remain unqualified. Missing source material measure leaves the whole group unsupported, never a guessed colour.",
    }
