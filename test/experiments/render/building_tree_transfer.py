"""Sparse first-hit group transfers; fractional source coverage, not volumetric density."""

from collections import defaultdict

import numpy as np
from building_coverage_field import sample_field, validate_field
from building_mass_lighting import shade
from building_mass_material_image import hit_materials
from current_visibility import project, raster
from triangle_image_reference import average_samples, image_rays


def compose_layers(rays, depth, colour, opacity, ties, count):
    rays = np.asarray(rays)
    depth = np.asarray(depth)
    colour = np.asarray(colour)
    opacity = np.asarray(opacity)
    if (
        colour.shape != (len(rays), 3)
        or len(depth) != len(rays)
        or len(opacity) != len(rays)
        or np.any((opacity < 0) | (opacity > 1))
        or not all(np.isfinite(v).all() for v in (depth, colour, opacity))
        or np.any(rays < 0)
        or np.any(rays >= count)
    ):
        raise ValueError("finite bounded premultiplied surface transfers required")
    c = np.zeros((count, 3))
    t = np.ones(count)
    if not len(rays):
        return c, t, 0
    order = np.lexsort((ties, depth, rays))
    rays = rays[order]
    colour, opacity = colour[order], opacity[order]
    unique, first, counts = np.unique(rays, return_index=True, return_counts=True)
    for rank in range(int(counts.max())):
        active = counts > rank
        at = first[active] + rank
        target = unique[active]
        weight = t[target] * opacity[at]
        c[target] += weight[:, None] * colour[at]
        t[target] *= 1 - opacity[at]
    return c, t, int(counts.max())


def layer_material(
    faces,
    points,
    normals,
    group,
    product,
    appearance,
    original,
    fields,
    original_faces,
    *,
    opaque,
    coverage=None,
):
    if group < 0:
        mapped = original_faces[faces - len(product["indices"])]
        colour, weights = hit_materials(original, fields, points, mapped)
        return colour, weights, np.ones(len(faces))
    role = (normals[:, 2] > 0.5).astype(int)
    colour = appearance["albedo"][group, role]
    weights = appearance["roughness_weights"][group, role]
    n = np.abs(normals)
    coefficient = product["group_coverage"][group][[2, 1, 0]]
    opacity = np.ones(len(faces)) if opaque else (n @ coefficient) / n.sum(axis=1)
    if coverage is not None and not opaque:
        opacity = sample_field(points, normals, coverage, group)
    return colour, weights, opacity


def validate_inputs(appearance, product, coverage):
    if (
        not appearance["supported"].all()
        or not np.isfinite(product["group_coverage"]).all()
        or np.any((product["group_coverage"] < 0) | (product["group_coverage"] > 1))
    ):
        raise ValueError("qualified finite source coverage/material measures required")
    if coverage is not None:
        validate_field(coverage, len(appearance["albedo"]))


def triangle_normals(candidate):
    triangles = candidate["positions"].astype(float)[candidate["indices"]]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    length = np.linalg.norm(normal, axis=1)
    return normal / np.where(length > 0, length, 1)[:, None]


def layer_groups(candidate, product, count):
    groups = [(-1, np.arange(len(product["indices"]), len(candidate["indices"])))]
    groups.extend(
        (g, np.flatnonzero(product["triangle_groups"] == g)) for g in range(count)
    )
    return groups


def group_hits(projected, forward, indices, faces, shape, directions, camera, mask):
    work = {}
    depth, owner = raster(
        projected, forward, indices[faces], *shape, with_faces=True, counters=work
    )
    hit = owner.ravel() >= 0
    if mask is not None:
        hit &= mask
    hit = np.flatnonzero(hit)
    face = faces[owner.ravel()[hit]]
    z = depth.ravel()[hit]
    points = camera[0] + (0.1 / z)[:, None] * directions[hit]
    return hit, face, z, points, work


def sample_owners(mask, count):
    if mask is None:
        return None
    mask = np.asarray(mask)
    if mask.dtype != np.bool_ or mask.shape != (count,):
        raise ValueError("one explicit boolean owner per quadrature sample required")
    return mask


def transfers(
    candidate,
    original,
    fields,
    product,
    appearance,
    camera,
    box,
    tables,
    supersample,
    *,
    opaque=False,
    coverage=None,
    light=(0.45, -0.5, 0.7),
    intensity=3,
    ambient=0.1,
    sample_mask=None,
):
    """Masked output is a partial C/T measure, with zero at unowned samples."""
    width, height = map(int, box[1] - box[0])
    sample_count = width * height * supersample**2
    if sample_count > 1048576:
        raise ValueError("bounded diagnostic ray count required")
    sample_mask = sample_owners(sample_mask, sample_count)
    validate_inputs(appearance, product, coverage)
    projected, forward = project(candidate["positions"], camera)
    if np.any(forward <= 0.1):
        raise ValueError("geometry must remain beyond near plane")
    projected[:, :2] = (projected[:, :2] - box[0]) * supersample
    _, directions = image_rays(camera, box, width, height, supersample)
    outgoing = -directions / np.linalg.norm(directions, axis=1)[:, None]
    normal = triangle_normals(candidate)
    original_faces = np.flatnonzero(
        np.isin(original["binding"][:, 1], product["needs_sources"])
    )
    groups = layer_groups(candidate, product, len(appearance["albedo"]))
    rays, depths, colours, alphas, ties = [], [], [], [], []
    lobes, peak, zero_samples, coverage_lookups = 0, 0, 0, 0
    raster_work = defaultdict(int)
    for group, faces in groups:
        if not len(faces):
            continue
        hit, face, z, points, traffic = group_hits(
            projected,
            forward,
            candidate["indices"],
            faces,
            (width * supersample, height * supersample),
            directions,
            camera,
            sample_mask,
        )
        for key, value in traffic.items():
            raster_work[key] += value
        if coverage is not None and not opaque and group >= 0:
            coverage_lookups += 3 * len(hit)
        rgb, weights, alpha = layer_material(
            face,
            points,
            normal[face],
            group,
            product,
            appearance,
            original,
            fields,
            original_faces,
            opaque=opaque,
            coverage=coverage,
        )
        nonzero = alpha > 0
        zero_samples += int(np.count_nonzero(~nonzero))
        if not nonzero.all():
            hit, face, z, alpha = (
                hit[nonzero],
                face[nonzero],
                z[nonzero],
                alpha[nonzero],
            )
            rgb, weights = rgb[nonzero], weights[nonzero]
        value, work = shade(
            rgb,
            weights,
            appearance["roughness_palette"],
            normal[face],
            outgoing[hit],
            light,
            tables,
            intensity=intensity,
            ambient=ambient,
        )
        rays.append(hit)
        depths.append(-z)
        colours.append(value)
        alphas.append(alpha)
        ties.append(face)
        lobes += work["ggx_lobe_evaluations"]
        peak = max(peak, work["maximum_row_array_bytes"])
    r = np.concatenate(rays) if rays else np.empty(0, int)
    d = np.concatenate(depths) if depths else np.empty(0)
    c = np.vstack(colours) if colours else np.empty((0, 3))
    a = np.concatenate(alphas) if alphas else np.empty(0)
    tie = np.concatenate(ties) if ties else np.empty(0, int)
    value, transmission, layers = compose_layers(r, d, c, a, tie, sample_count)
    if sample_mask is not None:
        transmission[~sample_mask] = 0
    return {
        "C": average_samples(value, width, height, supersample),
        "T": average_samples(transmission, width, height, supersample),
    }, {
        "layer_records": len(r),
        "zero_opacity_samples_skipped_before_shading": zero_samples,
        "coverage_projection_cell_lookups": coverage_lookups,
        "maximum_groups_per_ray": layers,
        "all_samples": sample_count,
        "ggx_lobe_evaluations": lobes,
        "maximum_row_array_bytes": peak,
        "known_sparse_layer_array_bytes": sum(x.nbytes for x in (r, d, c, a, tie)),
        "raster_work_summed_over_groups": dict(raster_work),
        "scope": "First hit per valid coarse group plus actual opaque original remainder, sorted by depth per ray. Independent fractional coverage is an approximation; no source mask correlation, hard-shadow, depth/occluder, native OIT or radiance-integral admission.",
    }
