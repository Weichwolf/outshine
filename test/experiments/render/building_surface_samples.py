"""Bounded six-view first-surface samples directly from analytic source patches."""

import numpy as np
import shapely
from building_mass_materials import material
from prepared_material_fields import edge_at
from source_material_patches import corner_fields
from source_shading_fields import wall_colours

SIZE = 16


def validate(surfels):
    count = len(surfels["positions"])
    palette = surfels["roughness_palette"]
    if (
        not 0 < count <= 1536
        or any(
            surfels[k].shape != (count, 3) for k in ("positions", "normal", "albedo")
        )
        or surfels["roughness_weights"].shape != (count, len(palette))
        or any(
            not np.isfinite(surfels[k]).all()
            for k in ("positions", "normal", "albedo", "roughness_weights", "spacing")
        )
        or surfels["spacing"].shape != ()
        or surfels["spacing"] <= 0
        or np.any(abs(np.linalg.norm(surfels["normal"], axis=1) - 1) > 1e-6)
        or np.any((surfels["albedo"] < 0) | (surfels["albedo"] > 1))
        or np.any(surfels["roughness_weights"] < 0)
        or np.any(
            abs(surfels["roughness_weights"].sum(axis=1) - 1)
            > 8 * np.finfo(np.float32).eps
        )
    ):
        raise ValueError(
            "finite bounded surface positions, individual normals and material masses required"
        )


def sample_surfaces(patches, bounds, *, all_depths=False):
    bounds = np.asarray(bounds, float)
    side = float(np.ptp(bounds, axis=0).max())
    if bounds.shape != (2, 3) or not np.isfinite(bounds).all() or side <= 0:
        raise ValueError("bounded nonempty source volume required")
    low = bounds.mean(axis=0) - side / 2
    spacing = side / SIZE
    grid = low + (np.arange(SIZE)[:, None] + 0.5) * spacing
    depths = np.stack(
        (np.full((3, SIZE * SIZE), np.inf), np.full((3, SIZE * SIZE), -np.inf))
    )
    owners = np.full(depths.shape, -1, np.int32)
    points = np.zeros((*depths.shape, 3))
    parameters = np.zeros((*depths.shape, 2))
    layers = []
    layer_count = 0
    work = {
        "source_patches": len(patches),
        "parallel_patch_axis_pairs": 0,
        "tested_patch_ray_pairs": 0,
        "inside_patch_hits": 0,
    }
    for index, patch in enumerate(patches):
        surface = patch.geometry
        inverse = np.linalg.pinv(surface.axes)
        for axis in range(3):
            if abs(surface.normal[axis]) <= 1e-12:
                work["parallel_patch_axis_pairs"] += 1
                continue
            other = [a for a in range(3) if a != axis]
            x, y = np.meshgrid(grid[:, other[0]], grid[:, other[1]])
            candidate = np.zeros((SIZE * SIZE, 3))
            candidate[:, other] = np.column_stack((x.ravel(), y.ravel()))
            candidate[:, axis] = (
                surface.normal @ surface.origin - candidate @ surface.normal
            ) / surface.normal[axis]
            local = (candidate - surface.origin) @ inverse
            hit = shapely.covers(surface.domain, shapely.points(local))
            work["tested_patch_ray_pairs"] += len(candidate)
            work["inside_patch_hits"] += int(hit.sum())
            if all_depths:
                layer_count += int(hit.sum())
                if layer_count > 65536:
                    raise ValueError("bounded diagnostic LDC intermediate exhausted")
                layers.append(
                    (
                        candidate[hit],
                        np.full(int(hit.sum()), index, np.int32),
                        local[hit],
                    )
                )
                continue
            for layer in range(2):
                nearer = (
                    candidate[:, axis] < depths[layer, axis]
                    if layer == 0
                    else candidate[:, axis] > depths[layer, axis]
                )
                selected = hit & nearer
                depths[layer, axis, selected] = candidate[selected, axis]
                owners[layer, axis, selected] = index
                points[layer, axis, selected] = candidate[selected]
                parameters[layer, axis, selected] = local[selected]
    hit = owners.ravel() >= 0
    result = {
        "positions": points.reshape(-1, 3)[hit],
        "patches": owners.ravel()[hit],
        "parameters": parameters.reshape(-1, 2)[hit],
    }
    if all_depths:
        result = {
            k: np.concatenate([v[i] for v in layers])
            for i, k in enumerate(("positions", "patches", "parameters"))
        }
    work.update(
        {
            "axis_rays": 3 * SIZE * SIZE,
            "maximum_front_back_records": 6 * SIZE * SIZE,
            "first_surface_records": int(hit.sum()) if not all_depths else None,
            "all_depth_records": layer_count if all_depths else None,
            "sampling_grid_spacing_m": spacing,
            "known_sampler_array_bytes": sum(
                v.nbytes for v in (depths, owners, points, parameters)
            ),
            "known_all_depth_array_bytes": sum(
                v.nbytes for layer in layers for v in layer
            ),
            "sampling_result_array_bytes": sum(v.nbytes for v in result.values()),
        }
    )
    return result, spacing, work


def point_material(patch, parameters, at):
    uv, _, code = corner_fields(patch, parameters)
    local_normal = patch.orientation * np.cross(*patch.local_axes)
    local_normal /= np.linalg.norm(local_normal)
    stream = int(local_normal[2] > 0.5)
    derivative = patch.local_axes[:, 2].copy()
    opening = False
    if patch.side != 1:
        a, b, _, party = edge_at(patch.shape, patch.side)
        opening = not (patch.gable or party) and np.linalg.norm(b - a) >= 1.9
        derivative /= patch.shape["floor_m"]
    gradient = np.linalg.lstsq(patch.geometry.axes, derivative, rcond=None)[0]
    slope = float(np.linalg.norm(gradient))
    paint = wall_colours(uv, stream, patch.surface["wall_colour"])[:, :3]
    colours, weights = [], []
    for i in range(len(parameters)):
        above = uv[i, 1] >= 1
        c, w = material(
            at,
            code,
            stream,
            paint[i],
            openings=bool(above and opening),
            storeys_per_m=slope if above else 0,
            slope=slope,
        )
        colours.append(c)
        weights.append(w)
    return np.asarray(colours), np.asarray(weights)


def emit_samples(patches, sampled, spacing, at):
    count = len(sampled["positions"])
    normal = np.empty((count, 3))
    colour = np.empty((count, 3))
    weights = np.empty((count, len(at["palette"])))
    for identity in np.unique(sampled["patches"]):
        selected = sampled["patches"] == identity
        patch = patches[identity]
        normal[selected] = patch.geometry.normal
        colour[selected], weights[selected] = point_material(
            patch, sampled["parameters"][selected], at
        )
    result = {
        "positions": sampled["positions"].astype(np.float32),
        "normal": normal.astype(np.float32),
        "albedo": colour.astype(np.float32),
        "roughness_weights": weights.astype(np.float32),
    }
    rows = np.column_stack([v for v in result.values()])
    _, unique = np.unique(rows, axis=0, return_index=True)
    unique.sort()
    if len(unique) > 1536:
        raise ValueError("bounded diagnostic surface product exhausted")
    result = {k: v[unique] for k, v in result.items()}
    result.update(
        {"spacing": np.array(spacing), "roughness_palette": np.asarray(at["palette"])}
    )
    return result, {
        "duplicate_records_removed": count - len(unique),
        "surface_samples": len(unique),
        "surface_array_bytes": sum(v.nbytes for v in result.values()),
        "maximum_sample_records": 1536,
    }
