"""Camera/light-independent wall/roof reflectance measures before fine meshes."""

import hashlib
import json

import numpy as np
import shapely
from building_mass_materials import material
from prepared_material_fields import edge_at
from source_material_patches import shape_material_patches


def policy_digest(at):
    raw = json.dumps(
        at["sources_sha256"], sort_keys=True, separators=(",", ":")
    ).encode()
    return hashlib.sha256(raw).digest()


def patch_measure(patch, at):
    shape = patch.shape
    local_normal = patch.orientation * np.cross(*patch.local_axes)
    local_normal /= np.linalg.norm(local_normal)
    stream = int(local_normal[2] > 0.5)
    jacobian = np.linalg.norm(np.cross(*patch.geometry.axes))
    colour = patch.surface["wall_colour"]
    paint = np.ones(3) if colour is None else np.asarray(colour) / at["base"][0]
    result = np.zeros(4 + len(at["palette"]))
    if patch.side == 1:
        trim = 2 if shape["roof_kind"] == 0 else 1
        code = -(1 + trim + 16 * shape["ident"])
        if int(np.float32(code)) != code:
            raise ValueError(
                "roof category cannot be represented by the source UV word"
            )
        slope = np.sqrt(max(0, 1 - local_normal[2] ** 2))
        colour, weights = material(at, code, stream, paint, slope=slope)
        area = patch.geometry.domain.area * jacobian
        result[:] = np.r_[colour, weights, 1] * area
        return result, 1
    a, b, entrance, party = edge_at(shape, patch.side)
    opening = not (patch.gable or party) and np.linalg.norm(b - a) >= 1.9
    code = shape["opening_style"] + len(at["fractions"]) * (
        (2 if entrance and not patch.gable else 0) + 3 * (shape["wall_variant"] % 8)
    )
    base = shape["seat_m"] + shape["foot_m"]
    x0, z0, x1, z1 = patch.geometry.domain.bounds
    calls = 0
    for above, low, high in ((False, z0, min(z1, base)), (True, max(z0, base), z1)):
        if high <= low:
            continue
        area = (
            shapely.intersection(
                patch.geometry.domain, shapely.box(x0, low, x1, high)
            ).area
            * jacobian
        )
        if area <= 0:
            continue
        colour, weights = material(
            at,
            code,
            stream,
            paint,
            openings=above and opening,
            storeys_per_m=(1 / shape["floor_m"] if above else 0),
        )
        result += np.r_[colour, weights, 1] * area
        calls += 1
    return result, calls


def source_measure(identity, surface, frame, at):
    if surface is None:
        raise ValueError("original source material forms unavailable")
    result = np.zeros((2, 4 + len(at["palette"])))
    patches, calls = 0, 0
    for part, shape in enumerate(surface["shapes"]):
        for patch in shape_material_patches(
            surface, shape, identity, part, frame["basis"], frame["anchor"]
        ):
            value, count = patch_measure(patch, at)
            result[int(patch.side == 1)] += value
            patches += 1
            calls += count
    if np.any(result[:, -1] <= 0) or not np.isfinite(result).all():
        raise ValueError("positive finite source wall/roof measures required")
    return result, patches, calls


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
