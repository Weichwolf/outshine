"""Fade local collar shading toward the canonical tube surfaces at their joins."""

import numpy as np
from plant_surface import solid_from_mesh
from spine_frames import unit
from surface_normal_transfer import interpolate_normals


def tube_reference(mesh):
    solid = solid_from_mesh(mesh)
    raw = solid.to_mesh64()
    vertices = np.asarray(raw.vert_properties)
    faces = np.asarray(raw.tri_verts, dtype=int)
    reference = {
        "vertices": vertices[:, :3],
        "faces": faces,
        "normals": vertices[faces, 3:6].reshape(-1, 3),
    }
    return solid, reference


def blend_collar_normals(mesh, references, parent_radius, child_radius):
    _, first = np.unique(mesh["faces"].ravel(), return_index=True)
    original_normals = mesh["normals"][first]
    field_normals = original_normals
    points = mesh["vertices"]
    distances = np.full((len(points), len(references)), np.inf)
    target_normals = np.zeros((len(points), len(references), 3))
    ray_length = 4 * parent_radius
    beginnings = (points + ray_length * field_normals).tolist()
    endings = (points - ray_length * field_normals).tolist()
    directions = field_normals.tolist()
    for column, (solid, reference) in enumerate(references):
        selected, positions, faces = [], [], []
        for index, (beginning, ending, direction) in enumerate(
            zip(beginnings, endings, directions, strict=True)
        ):
            best, chosen = float("inf"), None
            nx, ny, nz = direction
            for hit in solid.ray_cast(beginning, ending):
                normal = hit.normal
                if normal[0] * nx + normal[1] * ny + normal[2] * nz <= 0:
                    continue
                error = abs(hit.distance - 0.5)
                if error < best:
                    chosen, best = hit, error
            if chosen is not None:
                selected.append(index)
                positions.append(chosen.position)
                faces.append(chosen.face_id)
        if selected:
            positions = np.asarray(positions)
            distances[selected, column] = np.sum(
                (points[selected] - positions) * field_normals[selected], axis=1
            )
            target_normals[selected, column] = interpolate_normals(
                reference, positions, np.asarray(faces, dtype=int)
            )
    nearest = np.argmin(abs(distances), axis=1)
    row = np.arange(len(points))
    available = np.isfinite(distances[row, nearest])
    mixed = field_normals.copy()
    x = np.clip(
        distances[row[available], nearest[available]] / (0.3 * child_radius), 0, 1
    )
    weight = x * x * (3 - 2 * x)
    mixed[available] = unit(
        (1 - weight[:, None]) * target_normals[row[available], nearest[available]]
        + weight[:, None] * field_normals[available]
    )
    result_normals = original_normals.copy()
    result_normals = mixed
    return mesh | {"normals": result_normals[mesh["faces"]].reshape(-1, 3)}, {
        "queried_vertices": len(points),
        "referenced_vertices": int(np.count_nonzero(available)),
        "field_only_vertices": int(np.count_nonzero(~available)),
    }
