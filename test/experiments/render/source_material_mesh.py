"""Triangulate explicit material domains and carry their exact corner fields."""

import numpy as np
from packed_normals import pack
from prepared_material_fields import facade_catalog
from source_material_patches import corner_fields, permute_corner_fields
from source_plane_mesh import factorized_positions, triangulate


def known_vertex_rows(geometry, fields):
    """Exact XYZ/normal/UV/metres tuples; no colour or native normal equivalence."""
    return np.unique(
        np.column_stack(
            (
                geometry["positions"][geometry["indices"]]
                .reshape(-1, 3)
                .view(np.uint32),
                np.repeat(fields["face_normal_words"], 3),
                fields["corner_uv"].reshape(-1, 2).view(np.uint32),
                fields["corner_metres"].reshape(-1, 2).view(np.uint32),
            )
        ),
        axis=0,
    )


def material_mesh(patches):
    triangles, normals, owners, uvs, metres, codes = [], [], [], [], [], []
    work = {}
    catalog = facade_catalog()
    for identity, patch in enumerate(patches):
        local = triangulate(patch.geometry.domain, work)
        xyz = patch.geometry.origin + local @ patch.geometry.axes
        uv, material, code = corner_fields(patch, local.reshape(-1, 2), catalog=catalog)
        triangles.extend(xyz)
        normals.extend([patch.geometry.normal] * len(xyz))
        owners.extend([identity] * len(xyz))
        uvs.extend(uv.reshape(-1, 3, 2))
        metres.extend(material.reshape(-1, 3, 2))
        codes.append(code)
    if not 0 < len(triangles) <= 131072 or len(patches) > 65536:
        raise ValueError("bounded nonempty material mesh required")
    original = np.asarray(triangles)
    xyz = original.astype(np.float32)
    normals = np.asarray(normals)
    geometric = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    swap = np.einsum("ij,ij->i", geometric, normals) < 0
    indices = np.arange(xyz.size // 3).reshape(-1, 3)
    indices[swap] = indices[swap][:, [0, 2, 1]]
    control = factorized_positions(
        {"positions": xyz.reshape(-1, 3), "indices": indices}
    )
    fields = {
        "corner_uv": permute_corner_fields(np.asarray(uvs), swap),
        "corner_metres": permute_corner_fields(np.asarray(metres), swap),
        "face_normal_words": pack(normals),
        "face_patches": np.asarray(owners, np.uint16),
        "patch_sources": np.array([p.geometry.source for p in patches], np.uint32),
        "patch_parts": np.array([p.part for p in patches], np.uint16),
        "patch_sides": np.array([p.side for p in patches], np.uint16),
        "patch_gables": np.array([p.gable for p in patches], bool),
        "patch_material_codes": np.array(codes, np.int32),
    }
    expected = fields["patch_material_codes"][fields["face_patches"]]
    uv = fields["corner_uv"][:, :, 0]
    decoded = np.where(uv < 0, uv, np.floor(uv / 256))
    if not np.array_equal(decoded, np.broadcast_to(expected[:, None], decoded.shape)):
        raise ValueError("triangle spans encoded categorical material codes")
    rows = known_vertex_rows(control, fields)
    work.update(
        triangles=len(triangles),
        material_patches=len(patches),
        gable_patches=int(fields["patch_gables"].sum()),
        unique_position_normal_uv_metres_rows=len(rows),
        corner_field_bytes=fields["corner_uv"].nbytes + fields["corner_metres"].nbytes,
        maximum_float32_vertex_rounding_m=float(
            np.linalg.norm(xyz.astype(float) - original, axis=2).max()
        ),
        float32_degenerate_triangles=int(
            (np.linalg.norm(geometric, axis=1) == 0).sum()
        ),
        categorical_code_crossing_triangles=0,
    )
    return control, fields, work


def carry_fields(fields, swap):
    return fields | {
        key: permute_corner_fields(fields[key], swap)
        for key in ("corner_uv", "corner_metres")
    }
