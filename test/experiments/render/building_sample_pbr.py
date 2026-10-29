"""Existing dielectric GGX model on exact stored native source points.

This holds source geometry/material/coverage fixed while changing only the
model's outgoing direction. Native capture radiance is the positive control;
there is no independently relit native perspective source opponent.
"""

import json

import numpy as np
from building_sample_inputs import digest
from material_filter import direct_ggx


def energy_tables(directory):
    manifest = json.loads((directory / "energy-tables.reference.json").read_text())
    path = directory / "energy-tables.npz"
    if digest(path) != manifest["array_file_sha256"]:
        raise ValueError("Fixed existing GGX energy-table binding changed")
    with np.load(path, allow_pickle=False) as saved:
        tables = dict(saved)
    if (
        set(tables) != set(manifest["arrays"])
        or any(list(v.shape) != manifest["arrays"][k] for k, v in tables.items())
        or not all(np.isfinite(v).all() for v in tables.values())
        or tables["shape"].tolist() != [32, 16]
    ):
        raise ValueError("Complete finite owned GGX tables required")
    return tables, manifest


def source_points(grid, depth, mask):
    if (
        mask.dtype != bool
        or mask.ndim != 2
        or mask.shape != depth.shape
        or mask.shape[0] != mask.shape[1]
        or not 3 <= mask.shape[0] <= 512
        or not 0 < np.count_nonzero(mask) <= 262144
        or not np.isfinite(depth).all()
        or np.any((depth < 0) | (depth > 1))
        or not np.array_equal(mask, depth > 0)
    ):
        raise ValueError("Bounded registered opaque source points required")
    y, x = np.nonzero(mask)
    right, up = grid.axes()
    size, extent = len(mask), grid.half_extent_m
    return (
        np.asarray(grid.centre)
        + (((x + 0.5) / size * 2 - 1) * extent)[:, None] * right
        + ((1 - (y + 0.5) / size * 2) * extent)[:, None] * up
        + ((4 * depth[mask].astype(float) - 2) * extent)[:, None] * grid.toward_eye
    )


def point_responses(arrays, grid, view, tables):
    mask = arrays["source_depth"] > 0
    points = source_points(grid, arrays["source_depth"], mask)
    albedo = arrays["source_base_colour"][mask]
    roughness = arrays["source_roughness"][mask]
    normal = arrays["source_normal"][mask]
    if (
        albedo.shape != points.shape
        or normal.shape != points.shape
        or roughness.shape != (len(points),)
        or not all(np.isfinite(v).all() for v in (albedo, roughness, normal))
        or np.any((albedo < 0) | (albedo > 1))
        or np.any((roughness <= 0) | (roughness > 1))
        or np.any(np.linalg.norm(normal, axis=1) == 0)
        or np.any(arrays["source_metalness"][mask] != 0)
    ):
        raise ValueError("This control requires finite opaque dielectric material")
    rows = np.zeros((len(points), 20))
    rows[:, :3], rows[:, 3] = albedo, roughness
    rows[:, 4:7], rows[:, 7] = 0.04, 1
    rows[:, 8:11] = normal
    # The frozen native source query uses azimuth0/elevation45, white100,
    # no environment. The native Source helper's three materials are opaque,
    # isotropic dielectrics with the ordinary IOR1.5/specular factor1 defaults.
    rows[:, 12:15] = [0, 2**-0.5, -(2**-0.5)]
    result = {}
    for name, outgoing in (
        ("capture", grid.toward_eye),
        ("perspective", np.asarray(view.eye) - points),
    ):
        rows[:, 16:19] = outgoing
        lobes = direct_ggx(rows, tables)
        image = np.zeros((*mask.shape, 4), float)
        image[mask, :3] = (lobes[:, :3] + lobes[:, 4:7]) * 100
        image[mask, 3] = 1
        if not np.isfinite(image).all() or np.any(image < 0):
            raise ValueError("Existing GGX evaluator did not yield finite radiance")
        result[name] = image
    return result, {
        "source_points": len(points),
        "ggx_evaluations": 2 * len(points),
        "ggx_input_row_bytes": rows.nbytes,
        "point_position_bytes": points.nbytes,
        "output_field_bytes": sum(v.nbytes for v in result.values()),
        "model": "Existing direct_ggx, normalized actual stored normals, dielectricF0=.04/F90=1, native fixed white directional100/azimuth0/elevation45, no environment. No fitted colour/scale/normal/material parameter or RGB-based point selection.",
    }
