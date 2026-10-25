"""Projected source area and positive unaveraged-normal response profiles per cell."""

from dataclasses import replace

import numpy as np
import shapely
from building_mass_appearance import patch_measure
from shapely.affinity import affine_transform

PROFILE_LIMIT = 4
CELL_LIMIT = 32768


def validate_fields(data):
    planes, cells, profiles = (
        len(data["plane_origin"]),
        len(data["alpha"]),
        len(data["profile_normal"]),
    )
    materials, roughness = len(data["material_albedo"]), len(data["roughness_palette"])
    shapes = {
        "plane_origin": (planes, 3),
        "plane_axes": (planes, 2, 3),
        "plane_low": (planes, 2),
        "plane_shape": (planes, 2),
        "plane_cell_offsets": (planes + 1,),
        "cell_profile_offsets": (cells + 1,),
        "profile_normal": (profiles, 3),
        "profile_material": (profiles,),
        "profile_weight": (profiles,),
        "material_albedo": (materials, 3),
        "material_roughness_weights": (materials, roughness),
        "spacing": (),
    }
    if (
        planes > 128
        or cells > CELL_LIMIT
        or profiles > PROFILE_LIMIT * cells
        or materials > profiles
        or not roughness
    ):
        raise ValueError("bounded matte field dimensions required")
    if any(
        data[k].shape != shape or not np.isfinite(data[k]).all()
        for k, shape in shapes.items()
    ):
        raise ValueError("finite matching matte array shapes required")
    starts = data["plane_cell_offsets"].astype(np.int64)
    offsets = data["cell_profile_offsets"].astype(np.int64)
    counts = np.diff(offsets)
    if (
        starts[0] != 0
        or starts[-1] != cells
        or offsets[0] != 0
        or offsets[-1] != profiles
        or np.any(counts < 0)
        or np.any(counts > PROFILE_LIMIT)
        or not np.array_equal(
            np.diff(starts), np.prod(data["plane_shape"].astype(np.int64), axis=1)
        )
    ):
        raise ValueError("bounded contiguous plane/cell/profile ranges required")
    if (
        data["spacing"] <= 0
        or np.any(data["plane_shape"] <= 0)
        or np.any((data["alpha"] < 0) | (data["alpha"] > 1))
        or not np.isfinite(data["alpha"]).all()
        or np.any((data["alpha"] > 0) & (counts == 0))
    ):
        raise ValueError("covered matte cells require bounded source profiles")
    if not np.allclose(
        data["plane_axes"] @ data["plane_axes"].transpose(0, 2, 1),
        np.eye(2),
        atol=1e-10,
        rtol=0,
    ) or np.any(abs(np.linalg.norm(data["profile_normal"], axis=1) - 1) > 1e-6):
        raise ValueError(
            "orthonormal plane axes and individual unit source normals required"
        )
    if (
        np.any(data["profile_material"] < 0)
        or np.any(data["profile_material"] >= materials)
        or np.any(data["profile_weight"] <= 0)
        or np.any((data["material_albedo"] < 0) | (data["material_albedo"] > 1))
        or np.any(data["material_roughness_weights"] < 0)
    ):
        raise ValueError(
            "positive profile masses and physical material references required"
        )
    mass = np.bincount(
        np.repeat(np.arange(cells), counts),
        weights=data["profile_weight"],
        minlength=cells,
    )
    tolerance = 8 * np.finfo(np.float32).eps
    if np.any(abs(mass[counts > 0] - 1) > tolerance) or np.any(
        abs(data["material_roughness_weights"].sum(axis=1) - 1) > tolerance
    ):
        raise ValueError("unit source profile and roughness masses required")


def merge_equal_normal_profiles(data):
    normals, materials, weights, offsets = [], [], [], [0]
    combined_cells = 0
    for begin, end in zip(
        data["cell_profile_offsets"][:-1], data["cell_profile_offsets"][1:], strict=True
    ):
        groups = {}
        for i in range(int(begin), int(end)):
            normal = data["profile_normal"][i]
            key = normal.tobytes()
            material = int(data["profile_material"][i])
            row = np.r_[
                data["material_albedo"][material],
                data["material_roughness_weights"][material],
            ].astype(float)
            if key not in groups:
                groups[key] = []
            groups[key].append((normal, row, float(data["profile_weight"][i])))
        combined_cells += int(len(groups) < end - begin)
        for rows in groups.values():
            mass = sum(r[2] for r in rows)
            material = (
                rows[0][1] if len(rows) == 1 else sum(r[1] * r[2] for r in rows) / mass
            )
            normals.append(rows[0][0])
            materials.append(material)
            weights.append(mass)
        offsets.append(len(normals))
    rows = np.asarray(materials, np.float32).reshape(
        -1, 3 + len(data["roughness_palette"])
    )
    table, indices = np.unique(rows, axis=0, return_inverse=True)
    result = data | {
        "cell_profile_offsets": np.asarray(offsets, np.uint32),
        "profile_normal": np.asarray(normals, np.float32).reshape(-1, 3),
        "profile_material": indices.astype(np.uint32),
        "profile_weight": np.asarray(weights, np.float32),
        "material_albedo": table[:, :3],
        "material_roughness_weights": table[:, 3:],
    }
    validate_fields(result)
    return result, {
        "cells_with_identical_normal_merge": combined_cells,
        "profiles_before": len(data["profile_normal"]),
        "profiles_after": len(normals),
        "raw_arrays_before_bytes": sum(v.nbytes for v in data.values()),
        "raw_arrays_after_bytes": sum(v.nbytes for v in result.values()),
        "scope": "Only word-identical Float32 normals combine, after the existing four-profile selection. Full roughness measures retained. Fixed F0=.04/F90=1 unit-mass response is linear before Float32; emitted mixture mass and RGB require separate checks. No normal mean or quantization.",
    }


def plane_frame(normal, offset, centre):
    axis = np.eye(3)[int(np.argmin(abs(normal)))]
    u = np.cross(normal, axis)
    u /= np.linalg.norm(u)
    v = np.cross(normal, u)
    origin = centre + normal * (offset - normal @ centre)
    return origin, np.stack((u, v))


def affine_coefficients(matrix, offset):
    return [matrix[0, 0], matrix[1, 0], matrix[0, 1], matrix[1, 1], *offset]


def project_domain(patch, origin, axes):
    matrix = patch.geometry.axes @ axes.T
    offset = (patch.geometry.origin - origin) @ axes.T
    domain = affine_transform(
        patch.geometry.domain, affine_coefficients(matrix, offset)
    )
    inverse = np.linalg.inv(matrix)
    return domain, affine_coefficients(inverse, -offset @ inverse)


def response_profiles(pieces, members, patches, inverses, at):
    combined = {}
    material_calls = 0
    for piece, index in zip(pieces, members, strict=True):
        patch = patches[int(index)]
        local = shapely.intersection(
            affine_transform(piece, inverses[int(index)]), patch.geometry.domain
        )
        if local.area <= 0:
            continue
        clipped = replace(patch, geometry=replace(patch.geometry, domain=local))
        value, calls = patch_measure(clipped, at)
        material_calls += calls
        if value[-1] <= 0 or not np.isfinite(value).all():
            raise ValueError("positive clipped source material measure required")
        material = (value[:-1] / value[-1]).astype(np.float32)
        normal = np.asarray(patch.geometry.normal, np.float32)
        key = normal.tobytes() + material.tobytes()
        if key not in combined:
            combined[key] = [normal, material, 0.0]
        combined[key][2] += piece.area
    rows = sorted(combined.values(), key=lambda row: -row[2])
    total = sum(row[2] for row in rows)
    kept = rows[:PROFILE_LIMIT]
    mass = sum(row[2] for row in kept)
    if mass <= 0:
        raise ValueError("positive source profiles required for covered cell")
    profiles = [(normal, material, area / mass) for normal, material, area in kept]
    return profiles, {
        "material_calls": material_calls,
        "input_profiles": len(rows),
        "discarded_projected_area_m2": total - mass,
        "discarded_profile_fraction": (total - mass) / total,
    }


def plane_fields(patches, indices, normal, offset, centre, spacing, at):
    origin, axes = plane_frame(normal, offset, centre)
    projected, inverses = {}, {}
    for i in indices:
        projected[int(i)], inverses[int(i)] = project_domain(
            patches[int(i)], origin, axes
        )
    identities = np.array(list(projected), int)
    polygons = np.array(list(projected.values()), object)
    bounds = shapely.total_bounds(polygons).reshape(2, 2)
    low, high = (
        np.floor(bounds[0] / spacing).astype(int),
        np.ceil(bounds[1] / spacing).astype(int),
    )
    shape = high - low
    count = int(np.prod(shape))
    if np.any(shape <= 0) or count > CELL_LIMIT:
        raise ValueError("bounded nonempty plane grid required")
    x, y = np.meshgrid(np.arange(low[0], high[0]), np.arange(low[1], high[1]))
    cells = shapely.box(
        x.ravel() * spacing,
        y.ravel() * spacing,
        (x.ravel() + 1) * spacing,
        (y.ravel() + 1) * spacing,
    )
    pairs = shapely.STRtree(polygons).query(cells)
    order = np.lexsort((pairs[1], pairs[0]))
    cell, member = pairs[:, order]
    pieces = shapely.intersection(cells[cell], polygons[member])
    area = shapely.area(pieces)
    positive = area > 0
    cell, member, pieces = cell[positive], member[positive], pieces[positive]
    unique, first, counts = np.unique(cell, return_index=True, return_counts=True)
    alpha = np.zeros(count, np.float32)
    profiles = {}
    work = {
        "grid_cells": count,
        "candidate_cell_intersections": len(order),
        "positive_cell_intersections": len(pieces),
        "material_calls": 0,
        "cells_over_profile_budget": 0,
        "discarded_projected_area_m2": 0.0,
        "maximum_discarded_profile_fraction": 0.0,
        "double_cell_union_area_m2": 0.0,
    }
    for at_cell, begin, length in zip(unique, first, counts, strict=True):
        local_pieces = pieces[begin : begin + length]
        union = shapely.union_all(local_pieces)
        fraction = union.area / cells[at_cell].area
        if (
            not np.isfinite(fraction)
            or not 0 < fraction <= 1 + 64 * np.finfo(float).eps
        ):
            raise ValueError("bounded projected source coverage required")
        alpha[at_cell] = min(fraction, 1)
        profiles[int(at_cell)], stats = response_profiles(
            local_pieces,
            identities[member[begin : begin + length]],
            patches,
            inverses,
            at,
        )
        work["material_calls"] += stats["material_calls"]
        work["cells_over_profile_budget"] += int(
            stats["input_profiles"] > PROFILE_LIMIT
        )
        work["discarded_projected_area_m2"] += stats["discarded_projected_area_m2"]
        work["maximum_discarded_profile_fraction"] = max(
            work["maximum_discarded_profile_fraction"],
            stats["discarded_profile_fraction"],
        )
        work["double_cell_union_area_m2"] += union.area
    work.update(
        {
            "source_projection_union_area_m2": float(shapely.union_all(polygons).area),
            "float32_reconstructed_area_m2": float(
                np.sum(alpha.astype(float) * shapely.area(cells))
            ),
            "covered_cells": len(profiles),
        }
    )
    return {
        "origin": origin,
        "axes": axes,
        "low": low * spacing,
        "shape": shape,
        "alpha": alpha,
        "profiles": profiles,
    }, work


def build_fields(patches, cover, bounds, at):
    spacing = float(np.ptp(bounds, axis=0).max() / 16)
    fields, reports = [], []
    total = 0
    for i, (normal, offset) in enumerate(
        zip(cover["plane_normal"], cover["plane_offset"], strict=True)
    ):
        field, work = plane_fields(
            patches,
            np.flatnonzero(cover["patch_planes"] == i),
            normal,
            offset,
            bounds.mean(axis=0),
            spacing,
            at,
        )
        total += len(field["alpha"])
        if total > CELL_LIMIT:
            raise ValueError("whole root plane-cell budget exhausted")
        fields.append(field)
        reports.append(work)
    origins, axes, lows, shapes, alpha = [], [], [], [], []
    cell_start, profile_start = [0], [0]
    normals, materials, weights = [], [], []
    for field in fields:
        origins.append(field["origin"])
        axes.append(field["axes"])
        lows.append(field["low"])
        shapes.append(field["shape"])
        alpha.extend(field["alpha"])
        cell_start.append(len(alpha))
        for cell in range(len(field["alpha"])):
            for normal, material, weight in field["profiles"].get(cell, ()):
                normals.append(normal)
                materials.append(material)
                weights.append(weight)
            profile_start.append(len(normals))
    materials = np.array(materials, np.float32).reshape(-1, 3 + len(at["palette"]))
    table, indices = np.unique(materials, axis=0, return_inverse=True)
    arrays = {
        "plane_origin": np.array(origins).reshape(-1, 3),
        "plane_axes": np.array(axes).reshape(-1, 2, 3),
        "plane_low": np.array(lows).reshape(-1, 2),
        "plane_shape": np.array(shapes, np.uint32).reshape(-1, 2),
        "plane_cell_offsets": np.array(cell_start, np.uint32),
        "alpha": np.array(alpha, np.float32),
        "cell_profile_offsets": np.array(profile_start, np.uint32),
        "profile_normal": np.array(normals, np.float32).reshape(-1, 3),
        "profile_material": indices.astype(np.uint32),
        "profile_weight": np.array(weights, np.float32),
        "material_albedo": table[:, :3],
        "material_roughness_weights": table[:, 3:],
        "roughness_palette": np.array(at["palette"]),
        "spacing": np.array(spacing),
    }
    validate_fields(arrays)
    return arrays, {
        "planes": reports,
        "plane_count": len(fields),
        "cells": len(alpha),
        "profiles": len(normals),
        "distinct_materials": len(table),
        "raw_field_array_bytes": sum(v.nbytes for v in arrays.values()),
    }
