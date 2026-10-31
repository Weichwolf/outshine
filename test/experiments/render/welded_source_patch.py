"""Construct source-domain corners in the recorded native weld/storage stages.

This reference reuses native-rule UV/shading references. A different domain
triangulation does not establish original support, derivatives or World validity.
"""

import numpy as np
from compare_building_originals import native_normals
from prepared_material_fields import facade_catalog
from source_material_patches import corner_fields, round_away, weld_local
from source_plane_mesh import triangulate
from source_shading_fields import shading_fields


def stored_positions(patch, parameters, render_basis):
    local = patch.local_origin + parameters @ patch.local_axes
    snapped = weld_local(local)
    surface = patch.surface
    frame = np.array([surface[key] for key in ("east", "north", "up")], float)
    origin = np.array(surface["origin"], float)
    # BuildingMesh::Corner rounds the local-frame sum to StoredVertex Float32
    # before ConvertBuildingMesh applies the recorded capture frame.
    stored = (
        (
            (
                (origin + snapped[..., 0, None] * frame[0])
                + snapped[..., 1, None] * frame[1]
            )
            + snapped[..., 2, None] * frame[2]
        )
        .astype(np.float32)
        .astype(np.float64)
    )
    result = np.stack(
        [
            (stored[..., 0] * axis[0] + stored[..., 1] * axis[1])
            + stored[..., 2] * axis[2]
            for axis in render_basis
        ],
        axis=-1,
    ).astype(np.float32)
    if not np.isfinite(result).all():
        raise ValueError("Finite stored source corners required")
    return result, local


class TriangleBudget(ValueError):
    """Discard the incomplete candidate and use its whole original source."""


def patch_corners(patch, render_basis, *, max_triangles=None):
    work = {}
    parameters = triangulate(patch.geometry.domain, work)
    if max_triangles is not None and len(parameters) > max_triangles:
        raise TriangleBudget("Source triangle budget exhausted; retain whole original")
    local = patch.local_origin + parameters @ patch.local_axes
    expected = patch.orientation * np.cross(*patch.local_axes)
    reverse = (
        np.einsum(
            "ij,j->i",
            np.cross(local[:, 1] - local[:, 0], local[:, 2] - local[:, 0]),
            expected,
        )
        < 0
    )
    parameters[reverse] = parameters[reverse][:, [0, 2, 1]]
    catalog = facade_catalog()
    uv, metres, _ = corner_fields(patch, parameters.reshape(-1, 2), catalog=catalog)
    fields = shading_fields(patch, parameters, catalog=catalog, uv=uv)
    valid = fields["face_shading_valid"]
    positions, local = stored_positions(patch, parameters, render_basis)
    millimetres = round_away(local * 1000)
    if np.any(abs(millimetres) > 1e9):
        raise ValueError("Bounded exact local weld coordinates required")
    mm = millimetres.astype(np.int64)
    cross = np.cross(mm[:, 1] - mm[:, 0], mm[:, 2] - mm[:, 0])
    if np.any(np.any(cross != 0, axis=1) & ~valid):
        raise ValueError("Nondegenerate integer weld lost by floating normal rule")
    words = fields["corner_shading_words"][valid].reshape(-1)
    normals = native_normals(words, render_basis).reshape(-1, 3, 3).astype(np.float32)
    streams = fields["face_native_stream"][valid].copy()
    uv = uv.reshape(-1, 3, 2)[valid]
    metres = metres.reshape(-1, 3, 2)[valid]
    streams[(streams == 0) & np.any(metres != 0, axis=(1, 2))] = 2
    if np.any((streams < 0) | (streams > 2)):
        raise ValueError("Whole native material stream required")
    return {
        "positions": positions[valid],
        "normals": normals,
        "uv": uv,
        "uv1": metres,
        "colours": fields["corner_colour"][valid],
        "material": streams,
    }, {
        **work,
        "domain_triangles": len(parameters),
        "retained_triangles": int(valid.sum()),
        "native_rule_pruned_degenerate_triangles": int((~valid).sum()),
    }


def source_corners(patches, render_basis, *, max_triangles=None):
    if not 1 <= len(patches) <= 65536:
        raise ValueError("Bounded whole source patches required")
    if max_triangles is not None and (
        type(max_triangles) is not int or not 0 <= max_triangles <= 131072
    ):
        raise ValueError("Triangle limit must be an integer in [0, 131072]")
    parts, work = [], {}
    for patch in patches:
        remaining = (
            None
            if max_triangles is None
            else max_triangles - work.get("domain_triangles", 0)
        )
        if remaining == 0:
            raise TriangleBudget(
                "Source triangle budget exhausted; retain whole original"
            )
        part, counts = patch_corners(patch, render_basis, max_triangles=remaining)
        parts.append(part)
        for key, count in counts.items():
            work[key] = (
                max(work.get(key, 0), count)
                if key.startswith("maximum_")
                else work.get(key, 0) + count
            )
    result = {key: np.concatenate([part[key] for part in parts]) for key in parts[0]}
    if not 1 <= len(result["material"]) <= 131072:
        raise ValueError("Bounded complete nonempty source corner product required")
    return result, work
